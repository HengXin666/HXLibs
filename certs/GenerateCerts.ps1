Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Write-Host "生成自签CA根证书..."
# 临时配置文件
$caConfig = [System.IO.Path]::GetTempFileName()
@"
[ req ]
distinguished_name = req_distinguished_name
prompt = no
[ req_distinguished_name ]
C = CN
ST = HX
O = HX
CN = HX-RootCA
"@ | Set-Content -Encoding ASCII $caConfig

openssl genrsa -out ca.key 2048
openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt -config $caConfig

Remove-Item $caConfig

Write-Host "生成服务器私钥和CSR..."
$serverConfig = [System.IO.Path]::GetTempFileName()
@"
[ req ]
distinguished_name = req_distinguished_name
prompt = no
[ req_distinguished_name ]
C = CN
ST = HX
O = HX
CN = 127.0.0.1
"@ | Set-Content -Encoding ASCII $serverConfig

openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -config $serverConfig

Remove-Item $serverConfig

Write-Host "使用CA签发服务器证书..."
$extConfig = [System.IO.Path]::GetTempFileName()
@"
[ v3_req ]
subjectAltName = IP:127.0.0.1
"@ | Set-Content -Encoding ASCII $extConfig

openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial `
    -out server.crt -days 365 -sha256 -extfile $extConfig -extensions v3_req

Remove-Item $extConfig

Write-Host "生成完成，文件列表:"
Get-ChildItem -Path . -Include ca.*,server.* | Format-Table -AutoSize

Write-Host "说明:"
Write-Host "  ca.crt / ca.key        -> 根CA证书与私钥"
Write-Host "  server.crt / server.key -> 服务器证书与私钥"
Write-Host "  server.csr             -> 服务器证书签名请求"
