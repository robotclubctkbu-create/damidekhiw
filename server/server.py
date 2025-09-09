import os, asyncio, json
from dotenv import load_dotenv; load_dotenv()
from uploader import upload_queue, uploader_worker, normalize

HOST=os.getenv("HOST","0.0.0.0"); PORT=int(os.getenv("PORT","5555"))

async def handle_client(r: asyncio.StreamReader, w: asyncio.StreamWriter):
    mac_master=None; addr=w.get_extra_info("peername"); print("[+] conn",addr)
    try:
        while True:
            line=await r.readline()
            if not line: break
            msg=json.loads(line.decode().strip())
            if msg.get("type")=="hello":
                mac_master=msg.get("mac_master"); print("[srv] hello",mac_master)
                cmd={"type":"request_data","mode":"normal","per_page":1,"max_page":2,
                     "mac_slave_list":["11:22:33:44:55:66","22:33:44:55:66:77"]}
                w.write((json.dumps(cmd)+"\n").encode()); await w.drain()
            elif msg.get("type")=="uart_result":
                payload=msg.get("payload") or {}
                try:
                    await upload_queue.put(normalize(mac_master,payload))
                except Exception as e:
                    print("[srv] drop:",e,payload)
    finally:
        w.close(); await w.wait_closed(); print("[-] bye",addr)

async def main():
    s=await asyncio.start_server(handle_client,HOST,PORT)
    print(f"[srv] listening {HOST}:{PORT}")
    asyncio.create_task(uploader_worker())
    async with s: await s.serve_forever()

if __name__=="__main__": asyncio.run(main())
