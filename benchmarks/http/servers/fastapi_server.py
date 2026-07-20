import sys, os
from fastapi import FastAPI, WebSocket
from fastapi.responses import PlainTextResponse, JSONResponse, FileResponse
import uvicorn
app=FastAPI()
USERS={'users':[{'id':1001,'name':'Alice','active':True,'roles':['admin','editor']},{'id':1002,'name':'Bob','active':True,'roles':['viewer']},{'id':1003,'name':'Carol','active':False,'roles':['viewer','billing']}],'page':1,'page_size':20,'total':3}
ASSETS=os.getenv('BENCH_ASSET_DIR', '.')
@app.get('/')
async def hello(): return PlainTextResponse('Hello World!')
@app.get('/api/users')
async def users(): return JSONResponse(USERS)
@app.get('/page.html')
async def page(): return FileResponse(os.path.join(ASSETS, 'page.html'), media_type='text/html')
@app.get('/payload.bin')
async def payload(): return FileResponse(os.path.join(ASSETS, 'payload.bin'), media_type='application/octet-stream')
@app.websocket('/ws')
async def ws(sock: WebSocket):
    await sock.accept()
    while True:
        try: await sock.send_text(await sock.receive_text())
        except Exception: break
if __name__=='__main__':
    port=int(sys.argv[1]) if len(sys.argv)>1 else 18080
    workers=int(sys.argv[2]) if len(sys.argv)>2 else 1
    uvicorn.run('fastapi_server:app', host='127.0.0.1', port=port, workers=workers)
