---
on:
  workflow_run:
    workflows: ["Release Docker Image"]
    types: [completed]
    conclusion: [failure]
    branches: [main]

permissions:
  contents: write
  actions: write
  pull-requests: write
  copilot-requests: write

engine:
  id: copilot
  env:
    COPILOT_PROVIDER_BASE_URL: ${{ secrets.LONGCAT_BASE_URL }}
    COPILOT_MODEL: ${{ secrets.LONGCAT_MODEL_NAME }}
    COPILOT_PROVIDER_API_KEY: ${{ secrets.LONGCAT_API_KEY }}
    COPILOT_PROVIDER_TYPE: openai

network:
  allowed:
    - defaults
    - api.longcat.ai

safe-outputs:
  create-pull-request:

timeout-minutes: 30
---

# 自动修复 Release 构建失败

当 Release Docker Image workflow 失败时，分析失败原因并尝试自动修复，最多重试 3 次。

## 执行步骤

1. **获取失败日志**：使用 `gh run view --log-failed` 获取失败的 job 日志
2. **分析失败原因**：判断是否为可自动修复的错误（编译错误、测试失败、依赖问题等）
3. **检查重试次数**：查询当前 workflow run 历史，如果已重试 3 次则停止并创建 issue 说明
4. **尝试修复**：
   - 如果是代码错误，直接修复并提交
   - 如果是配置问题，修改配置文件
   - 如果是依赖问题，更新依赖版本
5. **创建修复 PR**：将修复提交到 PR，等待合并
6. **重新触发构建**：PR 合并后重新运行 Release Docker Image workflow

## 重试计数规则

- 使用 `gh run list --workflow=release.yml --limit 10` 查询最近的 run 历史
- 统计当前失败 run 之后的 run 数量，即为已重试次数
- 如果已重试次数 >= 3，停止自动修复，创建 issue 说明失败原因和重试历史

## 安全约束

- 只修复明确的代码错误，不改变功能逻辑
- 每次修复都要在 PR 描述中说明失败原因和修复方案
- 如果无法自动修复（如需要人工判断的设计问题），创建 issue 并 @mention 维护者
- 不要修改 CI/CD 配置本身（.github/workflows/）

## 输出要求

- 修复成功：创建 PR 并重新触发构建
- 修复失败：创建 issue 说明原因，包含失败日志摘要和已尝试的修复方案
