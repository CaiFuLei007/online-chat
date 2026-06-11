/* env.js — 前端环境占位符（部署时生成，请勿提交真实密码）
 *
 * 管理面板需要超管权限，密码不写死在代码中：
 *   部署时执行 frontend/gen-env.sh，由环境变量 PASSWORD 替换 __PASSWORD__ 占位符。
 *
 *   PASSWORD=xxx ADMIN_EMAIL=admin@example.com ./gen-env.sh
 */
window.ENV = {
  ADMIN_EMAIL: '__ADMIN_EMAIL__',   // 环境变量: ADMIN_EMAIL
  PASSWORD: '__PASSWORD__',         // 环境变量: PASSWORD
};
