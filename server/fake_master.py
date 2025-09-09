import asyncio, json, random, time, sys
HOST = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 5555
MASTER_MAC = "DEADBEEF1234"
SLAVES = ["112233445566", "A1B2C3D4E5F6", "FFEEDDCCBBAA"]

def one_reading(mac_slave):
    return {"type":"uart_result","mac_master":MASTER_MAC,
            "payload":{"mac-slave":mac_slave,"sensor":"temp",
                       "value":round(random.uniform(26.0,33.5),2),
                       "unit":"C","ts":int(time.time()),"page":1}}

async def run():
    reader, writer = await asyncio.open_connection(HOST, PORT)
    writer.write((json.dumps({"type":"hello","mac_master":MASTER_MAC})+"\n").encode()); await writer.drain()
    print(f"[sim] connected -> {HOST}:{PORT} as {MASTER_MAC}")
    try:
        while True:
            for mac in SLAVES:
                writer.write((json.dumps(one_reading(mac))+"\n").encode())
            await writer.drain()
            print("[sim] sent batch")
            await asyncio.sleep(2.0)
    finally:
        writer.close(); await writer.wait_closed()

if __name__ == "__main__":
    asyncio.run(run())
