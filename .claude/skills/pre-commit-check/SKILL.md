---
name: pre-commit-check
description: |
  ge 仓库本地 pre-commit 检查。在创建 PR 或推送代码前，必须运行本地 pre-commit 检查，确保 codespell、clang-format、end-of-file-fixer 等 hooks 全部通过，避免 CI `codecheck_precommit` 任务重复失败。
  **必须触发此 skill 的场景**（用户提到以下任何内容时使用）：
  - 创建/提交 PR：创建PR、提个PR、发PR、做个PR、帮我PR、生成PR、需要PR、pull request、merge request
  - 推送代码到远程：push代码、推代码、把代码推上去、提交到远程、推送到gitcode、提交代码到GitCode
  - 合并请求：合并请求、代码合入请求、请求合并、merge request
---

# ge 仓库 pre-commit 检查

## 背景

ge 仓库的 CI 流水线开启了 `codecheck_precommit` 任务，会对 PR 中改动的文件运行 pre-commit hooks（codespell、clang-format、end-of-file-fixer 等）。如果本地不提前检查，CI 会重复失败，浪费流水线资源。

## 何时执行

在 `git push` 之前，必须执行本地 pre-commit 检查。

## 执行方式

```bash
# 只检查本次改动的文件（推荐，快）
git diff --name-only HEAD~1 | xargs pre-commit run --files
```

如果 `pre-commit` 命令未安装，尝试通过 git hooks 触发：
```bash
git commit --amend --no-edit  # 触发 commit hooks
```

## 如果 pre-commit 失败

1. hooks 可能已自动修复格式问题（clang-format、end-of-file-fixer、trailing-whitespace 等），重新 `git add` 修复后的文件
2. codespell 等检查无法自动修复，需手动修正后重新 `git add`
3. 修正后 `git commit --amend --no-edit`
4. 重新运行 pre-commit 确认全部 Passed

```bash
# 修正后重新检查
git add -A
git commit --amend --no-edit
git diff --name-only HEAD~1 | xargs pre-commit run --files
```

## 禁止事项

**禁止使用 `--no-verify` 跳过 hooks**，否则 CI 会再次失败。

## 完成条件

**只有 pre-commit 全部 Passed 后才允许 push**。
