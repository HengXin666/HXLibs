import requests

proxies = {
    "http": "http://127.0.0.1:2334",
    "https": "http://127.0.0.1:2334"
}

try:
    response = requests.get("http://8.135.10.183:10106/", proxies=proxies, timeout=5)
    print("代理正常, 返回IP:", response.text)
except Exception as e:
    print("代理不可用:", e)
