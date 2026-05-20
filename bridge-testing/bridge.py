"""
bridge.py — reads JSON from the base node over USB serial and
forwards it to the browser via WebSocket. Also serves index.html.

Install deps:  pip install pyserial websockets
"""

import serial
import json
import asyncio
import websockets
import threading
import time
import os
from http.server import HTTPServer, SimpleHTTPRequestHandler
import logging

SERIAL_PORT = "/dev/tty.usbmodem101"   # base node
BAUD_RATE   = 115200
WS_PORT     = 8765
HTTP_PORT   = 8080

os.chdir(os.path.dirname(os.path.abspath(__file__)))
logging.basicConfig(level=logging.INFO, format="%(message)s")

clients: set = set()
_loop: asyncio.AbstractEventLoop | None = None


def serial_thread() -> None:
    """Reads from the base node serial port and broadcasts to WebSocket."""
    port = None
    while True:
        try:
            if port is None or not port.is_open:
                logging.info(f"Opening {SERIAL_PORT} ...")
                port = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
                logging.info("Serial port open.")

            raw = port.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="ignore").strip()
            data = json.loads(line)
            if _loop:
                asyncio.run_coroutine_threadsafe(
                    broadcast(json.dumps(data)), _loop
                )

        except json.JSONDecodeError:
            pass
        except serial.SerialException as e:
            logging.warning(f"Serial error: {e}")
            if port:
                try:
                    port.close()
                except Exception:
                    pass
                port = None
            logging.info("Retrying in 2 s...")
            time.sleep(2)
        except Exception as e:
            logging.warning(f"Unexpected error: {e}")
            time.sleep(1)


def http_thread() -> None:
    server = HTTPServer(("localhost", HTTP_PORT), SimpleHTTPRequestHandler)
    logging.info(f"HTTP server at http://localhost:{HTTP_PORT}/index.html")
    server.serve_forever()


async def broadcast(msg: str) -> None:
    if not clients:
        return
    await asyncio.gather(*[c.send(msg) for c in list(clients)],
                         return_exceptions=True)


async def ws_handler(websocket) -> None:
    clients.add(websocket)
    logging.info(f"Browser connected ({len(clients)} client(s))")
    try:
        await websocket.wait_closed()
    finally:
        clients.discard(websocket)
        logging.info(f"Browser disconnected ({len(clients)} client(s))")


async def main() -> None:
    global _loop
    _loop = asyncio.get_running_loop()

    threading.Thread(target=serial_thread, daemon=True).start()
    threading.Thread(target=http_thread, daemon=True).start()

    async with websockets.serve(ws_handler, "localhost", WS_PORT):
        logging.info(f"WebSocket server listening on ws://localhost:{WS_PORT}")
        await asyncio.Future()


if __name__ == "__main__":
    asyncio.run(main())
