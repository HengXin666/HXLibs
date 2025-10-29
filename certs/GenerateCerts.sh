#!/bin/bash
set -e

echo "生成自签CA根证书..."
openssl genrsa -out ca.key 2048
openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt \
  -subj "/C=CN/ST=HX/O=HX/CN=HX-RootCA"

echo "生成服务器私钥和CSR..."
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr \
  -subj "/C=CN/ST=HX/O=HX/CN=127.0.0.1"

echo "使用CA签发服务器证书..."
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days 365 -sha256 \
  -extfile <(echo "subjectAltName=IP:127.0.0.1")

echo "生成完成，文件列表:"
ls -l ca.* server.*
echo "说明:"
echo "  ca.crt / ca.key        -> 根CA证书与私钥"
echo "  server.crt / server.key -> 服务器证书与私钥"
echo "  server.csr             -> 服务器证书签名请求"
