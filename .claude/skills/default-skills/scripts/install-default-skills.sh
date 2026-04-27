#!/bin/bash
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

set -e

# 脚本功能：安装项目必备的 skills
# 默认技能列表
DEFAULT_SKILLS=("gitcode-pr" "gitcode-issue" "api-doc-generator")

# 脚本所在目录的上上级目录为 .claude/skills/
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SKILLS_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
REMOTE_DIR="$SKILLS_DIR/_remote"
GITIGNORE="$(cd "$SCRIPT_DIR/../../.." && pwd)/.gitignore"

# 创建远程 skills 子目录
mkdir -p "$REMOTE_DIR"

# 创建临时目录
TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT

# 克隆 skills 仓库
echo "Cloning skills repository..."
git clone --depth 1 https://gitcode.com/cann-agent/skills.git "$TEMP_DIR/skills"
if [ $? -ne 0 ]; then
    echo "Error: Failed to clone skills repository"
    exit 1
fi

# 检查 skills 目录是否存在
if [ ! -d "$TEMP_DIR/skills/skills" ]; then
    echo "Error: skills directory not found in repository"
    exit 1
fi

# 拷贝技能到 .claude/skills/_remote/ 目录，并在 skills 根目录创建符号链接
echo "Installing skills..."
for skill in "${DEFAULT_SKILLS[@]}"; do
    if [ -d "$TEMP_DIR/skills/skills/$skill" ]; then
        cp -r "$TEMP_DIR/skills/skills/$skill" "$REMOTE_DIR/"
        # 创建符号链接使 Claude Code 能发现 _remote 下的 skill（仅扫描一级目录）
        ln -sfn "_remote/$skill" "$SKILLS_DIR/$skill"
        echo "Installed skill: $skill"
    else
        echo "Warning: Skill '$skill' not found in repository"
    fi
done

# 更新 .gitignore：确保符号链接被忽略（幂等，已有条目不会重复添加）
if [ -f "$GITIGNORE" ]; then
    for skill in "${DEFAULT_SKILLS[@]}"; do
        ENTRY=".claude/skills/$skill"
        if ! grep -qxF "$ENTRY" "$GITIGNORE"; then
            echo "$ENTRY" >> "$GITIGNORE"
        fi
    done
fi

echo "All skills installed successfully."
