-- 工时查询模板（按月）
-- 可用占位符：{{.Year}} {{.Month}} {{.EmployeeNo}} {{.Start}} {{.End}}
--   Start / End 形如 2026-09-01 / 2026-09-30（当月首末日）
-- 要求：结果至少包含「日期列」和「工时列」，
--   日期列可命名为 date / work_date / day / workday / dt（否则取第 1 列）
--   工时列可命名为 hours / work_hours / duration / worktime（否则取第 2 列）
-- 下面只是示例，请按公司实际表结构替换。

SELECT
    DATE_FORMAT(t.work_date, '%Y-%m-%d') AS `date`,
    SUM(t.work_hours)                    AS hours
FROM pingcode_worktime t
WHERE t.employee_no = '{{.EmployeeNo}}'
  AND t.work_date BETWEEN '{{.Start}}' AND '{{.End}}'
GROUP BY DATE_FORMAT(t.work_date, '%Y-%m-%d')
ORDER BY `date`
