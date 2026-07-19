import sys
from fastapi import FastAPI, WebSocket
from fastapi.responses import PlainTextResponse, HTMLResponse
import uvicorn
app=FastAPI(); HTML='<!doctype html><html><body><h1>HXLibs benchmark</h1></body></html>'
@app.get('/')
async def hello(): return PlainTextResponse('Hello World!')
@app.get('/page.html')
async def page(): return HTMLResponse(HTML)
@app.websocket('/ws')
async def ws(sock: WebSocket):
    await sock.accept()
    while True:
        try: await sock.send_text(await sock.receive_text())
        except Exception: break
if __name__=='__main__': uvicorn.run(app, host='127.0.0.1', port=int(sys.argv[1]) if len(sys.argv)>1 else 18080)
