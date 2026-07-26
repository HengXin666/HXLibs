import os
import sys

from fastapi import FastAPI, WebSocket
from fastapi.responses import FileResponse, JSONResponse, PlainTextResponse, Response
import uvicorn

# FastAPI 最佳实践: orjson 序列化 + uvloop 事件循环 + httptools 解析器,
# 缺一个都会让结果严重低估 FastAPI 的真实水平。
try:
    from fastapi.responses import ORJSONResponse as DefaultJSON
except ImportError:  # orjson 未安装时退化, 但压测环境应始终安装
    DefaultJSON = JSONResponse

try:
    import uvloop  # noqa: F401
    LOOP = 'uvloop'
except ImportError:
    LOOP = 'auto'

try:
    import httptools  # noqa: F401
    HTTP = 'httptools'
except ImportError:
    HTTP = 'auto'

USERS = {'users': [
    {'id': 1001, 'name': 'Alice', 'active': True, 'roles': ['admin', 'editor']},
    {'id': 1002, 'name': 'Bob', 'active': True, 'roles': ['viewer']},
    {'id': 1003, 'name': 'Carol', 'active': False, 'roles': ['viewer', 'billing']},
], 'page': 1, 'page_size': 20, 'total': 3}
ASSETS = os.getenv('BENCH_ASSET_DIR', '.')

# 静态内容启动时读入内存并复用同一个 Response 对象, 避免每请求的文件 IO 与对象构造
with open(os.path.join(ASSETS, 'page.html'), 'rb') as f:
    PAGE = Response(f.read(), media_type='text/html; charset=utf-8')
with open(os.path.join(ASSETS, 'payload.bin'), 'rb') as f:
    PAYLOAD = Response(f.read(), media_type='application/octet-stream')
HELLO = PlainTextResponse('Hello World!')
USERS_RESPONSE = DefaultJSON(USERS)

app = FastAPI(default_response_class=DefaultJSON)


@app.get('/')
async def hello():
    return HELLO


@app.get('/api/users')
async def users():
    return USERS_RESPONSE


@app.get('/api/users/{user_id}/orders/{order_id}')
async def route_query(user_id: int, order_id: int, page: int, limit: int, sort: str):
    return {'user_id': user_id, 'order_id': order_id, 'page': page, 'limit': limit, 'sort': sort}


@app.get('/page.html')
async def page():
    return PAGE


@app.get('/payload.bin')
async def payload():
    return PAYLOAD


# 磁盘变体: 用 FastAPI 原生 FileResponse, 每请求真实读盘
@app.get('/page-file.html')
async def page_file():
    return FileResponse(os.path.join(ASSETS, 'files', 'page-file.html'),
                        media_type='text/html; charset=utf-8')


@app.get('/payload-file.bin')
async def payload_file():
    return FileResponse(os.path.join(ASSETS, 'files', 'payload-file.bin'),
                        media_type='application/octet-stream')


@app.websocket('/ws')
async def ws(sock: WebSocket):
    await sock.accept()
    while True:
        try:
            await sock.send_text(await sock.receive_text())
        except Exception:
            break


if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 18080
    workers = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    uvicorn.run('fastapi_server:app', host='127.0.0.1', port=port, workers=workers,
                loop=LOOP, http=HTTP, access_log=False, log_level='warning')
