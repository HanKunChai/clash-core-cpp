#!/bin/bash

# 定义路径
CLASH_BIN="./bin/clash-core-cpp"
LOG_FILE="$HOME/.config/clash/clash.log"

# 确保日志文件存在
mkdir -p "$(dirname "$LOG_FILE")"
touch "$LOG_FILE"

# 清空旧日志以便观察
echo "" > "$LOG_FILE"

echo "Starting Clash Core..."
# 启动 Clash (后台运行)
$CLASH_BIN > "$LOG_FILE" 2>&1 &
CLASH_PID=$!

# 等待启动
sleep 2

echo "Clash started with PID $CLASH_PID"
echo "Monitoring log file: $LOG_FILE"

# 启动日志监控 (后台)
tail -f "$LOG_FILE" &
TAIL_PID=$!

echo "Testing connection to www.google.com via proxy (127.0.0.1:7890)..."

# 测试连接
# -x: 代理地址
# -I: 仅获取头部
# -L: 跟随重定向
# --connect-timeout: 连接超时
curl -x http://127.0.0.1:7890 -I https://www.google.com --connect-timeout 15 

EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
    echo "Connection successful!"
else
    echo "Connection failed with exit code $EXIT_CODE"
fi

# 清理
echo "Stopping Clash..."
kill $CLASH_PID
kill $TAIL_PID

# 显示最后的日志摘要
echo "--- Last 10 lines of log ---"
tail -n 10 "$LOG_FILE"
