#!/usr/bin/env sh
# gen-env.sh — 从环境变量生成 js/env.js（部署时执行）
#   用法: PASSWORD=xxx ADMIN_EMAIL=admin@example.com ./gen-env.sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"

: "${PASSWORD:?环境变量 PASSWORD 未设置}"
ADMIN_EMAIL="${ADMIN_EMAIL:-admin@example.com}"

cat > "$DIR/js/env.js" <<EOF
/* env.js — 由 gen-env.sh 自动生成，请勿手动编辑/提交 */
window.ENV = {
  ADMIN_EMAIL: '$ADMIN_EMAIL',
  PASSWORD: '$PASSWORD',
};
EOF

echo "已生成 $DIR/js/env.js"
