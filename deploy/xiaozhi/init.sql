-- =============================================================================
-- xiaozhi-esp32-server 数据库初始化（GreatSQL 8.x / MySQL 8.x）
--
-- 用法（在 GreatSQL 所在机器，用 root 执行）：
--   mysql -uroot -p < init.sql
--
-- !! 重要：本脚本故意不建表、不写初始数据 !!
-- console（manager-api）容器首次启动时，通过内置 Liquibase 在本库自动执行
-- 上游全部变更集（约 90 个 changelog：DROP/CREATE 建表 + INSERT 初始数据，
-- 见上游 main/manager-api/src/main/resources/db/changelog/）。
-- 若在此手工建表，Liquibase 首次启动会因 DATABASECHANGELOG 无记录而重放全部
-- 变更集，撞上已存在的表/重复主键直接启动失败，还可能清掉手工数据。
--
-- 密码必须与 deploy/xiaozhi/docker-compose.yml 中
-- SPRING_DATASOURCE_DRUID_PASSWORD 保持一致（当前占位符相同，方便一次替换）。
-- =============================================================================

-- 1. 建库（与上游 changelog 的 utf8mb4 表定义配套）
CREATE DATABASE IF NOT EXISTS xiaozhi_esp32_server
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_unicode_ci;

-- 2. 建小智专用用户
-- 容器经 host.docker.internal 访问宿主机，来源 IP 是 Docker 网桥地址，
-- 故 host 用 '%'；权限只限本库，不给他库。
-- 若 GreatSQL 开启强密码策略报错 1819，换成含特殊字符的强密码，
-- 并同步修改 docker-compose.yml。
CREATE USER IF NOT EXISTS 'xiaozhi'@'%'
  IDENTIFIED BY 'CHANGE_ME_MYSQL_PASSWORD';

-- 已有用户需要改密码时执行（取消注释）：
-- ALTER USER 'xiaozhi'@'%' IDENTIFIED BY 'CHANGE_ME_MYSQL_PASSWORD';

-- 3. 授权：仅 xiaozhi_esp32_server 库。
-- 需要 ALL（含 CREATE/ALTER/DROP/INDEX）：console 首次启动及后续升级
-- 都要通过 Liquibase 建表改表。
GRANT ALL PRIVILEGES ON xiaozhi_esp32_server.* TO 'xiaozhi'@'%';

FLUSH PRIVILEGES;

-- =============================================================================
-- 4. 首次启动 console 后的验收（此时执行，取消注释）：
--
-- 变更集是否全部执行（行数应与上游 db.changelog-master.yaml 条目数一致）：
-- SELECT COUNT(*) FROM xiaozhi_esp32_server.DATABASECHANGELOG;
-- 建出的表：
-- SELECT table_name, table_comment FROM information_schema.tables
--  WHERE table_schema = 'xiaozhi_esp32_server' ORDER BY table_name;
-- 初始数据抽查（参数字典）：
-- SELECT COUNT(*) FROM xiaozhi_esp32_server.sys_params;
--
-- console 日志应出现 "Started AdminApplication" 且无 JDBC/Liquibase 报错。
-- =============================================================================
