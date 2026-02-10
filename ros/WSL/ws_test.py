import asyncio
import websockets
import json

async def test():
    uri = "ws://localhost:9090"
    async with websockets.connect(uri) as ws:
        # 목표 위치 전송
        msg = {
            "type": "goal_pose",
            "timestamp": 1234567890.0,
            "data": {"x": 2.5, "y": 1.3, "theta": 1.57}
        }
        await ws.send(json.dumps(msg))
        
        # 메시지 수신
        while True:
            data = await ws.recv()
            print(json.loads(data))

asyncio.run(test())
