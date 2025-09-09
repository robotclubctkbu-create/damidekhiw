import os, asyncio, time, random, aiohttp
from dotenv import load_dotenv; load_dotenv()
WEB_BASE=os.getenv("WEB_BASE","http://127.0.0.1:8000")
WEB_TOKEN=os.getenv("WEB_TOKEN","dev-token")
ENDPOINT="/api/telemetry/temperature"
BATCH_SIZE=int(os.getenv("BATCH_SIZE","25")); BATCH_DELAY=float(os.getenv("BATCH_DELAY","0.5"))
upload_queue: asyncio.Queue = asyncio.Queue()

def normalize(mac_master, payload):
    return {
        "mac_master": mac_master,
        "mac_slave": payload.get("mac-slave") or payload.get("mac_slave"),
        "value": float(payload.get("value")),
        "unit": payload.get("unit","C"),
        "ts": int(payload.get("ts") or time.time()),
        "rssi": payload.get("rssi"), "page": payload.get("page"),
        "firmware": payload.get("firmware","master-1.0.0")
    }

async def uploader_worker():
    headers={"Authorization":f"Bearer {WEB_TOKEN}","Content-Type":"application/json"}
    async with aiohttp.ClientSession(base_url=WEB_BASE,headers=headers,timeout=aiohttp.ClientTimeout(total=30)) as s:
        while True:
            batch=[await upload_queue.get()]
            try:
                while len(batch)<BATCH_SIZE: batch.append(upload_queue.get_nowait())
            except asyncio.QueueEmpty: pass
            for attempt in range(6):
                try:
                    async with s.post(ENDPOINT,json={"records":batch}) as r:
                        if r.status in (200,201,202): break
                        raise RuntimeError(f"HTTP {r.status}: {await r.text()}")
                except Exception as e:
                    d=min(15,(2**attempt)+random.random()); print("[upload]",e,"retry",d,"s"); await asyncio.sleep(d)
            await asyncio.sleep(BATCH_DELAY)
