import serial
import json
import asyncio
import socket
import websockets
import threading
import time
import os
from http.server import HTTPServer, SimpleHTTPRequestHandler
import logging
import paho.mqtt.client as mqtt

# Serial comms
#SERIAL_PORT = "/dev/tty.usbmodem11401"
#SERIAL_PORT = "/dev/ttyACM0"
SERIAL_PORT = "/dev/cu.usbserial-54260615441"
#SERIAL_PORT = "COM8"
BAUD_RATE   = 115200
# Webpage comms
WS_PORT     = 6767
HTTP_PORT   = 8000
# MQTT comms
BROKER = "localhost"
#BROKER = "172.27.128.1"
TOPIC = "prometheus/gesture"

# Global for storing any newly incoming serial data
new_data = {
    "player": 0,
    "gy": 0,
    "gz":0
}

_port = None
_port_lock = threading.Lock()

os.chdir(os.path.dirname(os.path.abspath(__file__)))
logging.basicConfig(level=logging.INFO, format="%(message)s")

clients: set = set()
_loop: asyncio.AbstractEventLoop | None = None


async def broadcast(msg: str) -> None:
    if not clients:
        return
    print("Abt to send data to game")
    await asyncio.gather(*[c.send(msg) for c in list(clients)],
                         return_exceptions=True)


async def websocket_handler(websocket) -> None:
    clients.add(websocket)
    logging.info(f"Browser connected ({len(clients)} client(s))")
    try:
        async for msg in websocket:
            try:
                data = json.loads(msg)

                if data.get("type") in ["audio", "score", "gesture"]:
                    serial_write(json.dumps(data) + "\r\n")
            except json.JSONDecodeError:
                print("womp womp invalid json from game")
    finally:
        clients.discard(websocket)
        logging.info(f"Browser disconnected ({len(clients)} client(s))")

def mqtt_msg_handler(client, userdata, msg):

    message = msg.payload.decode()
    print("Received:", message)

    gesture_packet = {
        "type": "gesture",
        "gesture": message
    }

    serial_write(json.dumps(gesture_packet) + "\r\n")

    if _loop:
        asyncio.run_coroutine_threadsafe(
            broadcast(json.dumps(gesture_packet)),
            _loop
        )
            
def serial_thread() -> None:
    """Reads from the base node serial port and broadcasts to WebSocket."""
    global _port
    while True:
        with _port_lock:
            try:
                if _port is None or not _port.is_open:
                    logging.info(f"Opening {SERIAL_PORT} ...")
                    _port = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
                    logging.info("Serial port open.")

                raw = _port.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="ignore").strip()
                print(f"From Base: {line}")
                data = json.loads(line)
                if _loop:
                    asyncio.run_coroutine_threadsafe(
                        broadcast(json.dumps(data)), _loop
                    )

            except json.JSONDecodeError:
                pass
            except serial.SerialException as e:
                logging.warning(f"Serial error: {e}")
                if _port:
                    try:
                        _port.close()
                    except Exception:
                        pass
                    _port = None
                logging.info("Retrying in 2 s...")
                time.sleep(2)
            except Exception as e:
                logging.warning(f"Unexpected error: {e}")
                time.sleep(1)

def serial_write(data: str) -> None:
    global _port
    with _port_lock:
        if _port and _port.is_open:
            try:
                _port.write(data.encode('utf-8'))
                # Flush the port to ensure packet is sent immediately
                _port.flush()
                print(f"To Base: {data!r}")
            except Exception as e:
                print(f"waa serial write didnt work {e}")
        else:
            print("tried sending to base nnode, but port aint even open")

def http_thread() -> None:
    server = HTTPServer(("localhost", HTTP_PORT), SimpleHTTPRequestHandler)
    logging.info(f"HTTP server at http://localhost:{HTTP_PORT}/index.html")
    server.serve_forever()

def mqtt_thread() -> None:
    try:
        # create a new client to connect to broker
        mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        mqtt_client.on_message = mqtt_msg_handler

        # make connection to broker and subscribe to prometheus gestures
        mqtt_client.connect(BROKER, 1883)
        print("MQTT 1883")
        mqtt_client.socket().setsockopt(
                socket.IPPROTO_TCP, socket.TCP_NODELAY, 1
            )
        logging.info("MQTT connected")
        mqtt_client.subscribe(TOPIC)
        print("subbed to prometheus/gestures")

        mqtt_client.loop_forever()

    except ConnectionRefusedError:
            print("MQTT broker not available, retrying in 2s...")
            time.sleep(2)
    except Exception as e:
        print("MQTT crashed:", e)
        time.sleep(2)



async def main() -> None:
    global _loop
    _loop = asyncio.get_running_loop()

    threading.Thread(target=serial_thread, daemon=True).start()
    threading.Thread(target=http_thread, daemon=True).start()
    threading.Thread(target=mqtt_thread, daemon=True).start()

    async with websockets.serve(websocket_handler, "localhost", WS_PORT):
        logging.info(f"WebSocket server listening on ws://localhost:{WS_PORT}")
        await asyncio.Future()


if __name__ == "__main__":
    asyncio.run(main())

