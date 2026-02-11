import asyncio
import websockets
import json

async def test():
    uri = "ws://localhost:9090"
    async with websockets.connect(uri) as ws:
        # 목표 위치 전송
        msg = {
            "type": "cmd_vel",
            "timestamp": 1234567890.0,
			"data": {
              "linear_x": 0.3,
              "angular_z": 0.5
            }
        }
        await ws.send(json.dumps(msg))
        
        # 메시지 수신
        while True:
            data = await ws.recv()
            print(json.loads(data))

asyncio.run(test())
