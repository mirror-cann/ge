#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

"""Pytest coverage for Python custom op API and bridge descriptor protocol."""

import importlib
import textwrap
from pathlib import Path

import pytest

try:
    bootstrap = importlib.import_module("ge.custom_op.bootstrap")
    bridge = importlib.import_module("ge.custom_op._bridge")
    custom_op = importlib.import_module("ge.custom_op")
except ImportError as exc:
    pytest.skip(f"无法导入 Python custom op 相关模块: {exc}", allow_module_level=True)


@pytest.fixture(autouse=True)
def clear_python_custom_op_runtime(monkeypatch):
    monkeypatch.delenv(bootstrap.ENV_PY_CUSTOM_OP_PATH, raising=False)
    custom_op.clear_registered_op_impls()
    bridge.clear_op_impl_holders()
    bridge.clear_loaded_op_impl_modules()
    yield
    bridge.clear_op_impl_holders()
    bridge.clear_loaded_op_impl_modules()
    custom_op.clear_registered_op_impls()


def _write_custom_op_module(
    dir_path: Path, module_name: str, class_name: str, op_type: str
) -> Path:
    file_path = dir_path / f"{module_name}.py"
    file_path.write_text(
        textwrap.dedent(f"""
        from ge.custom_op import EagerExecuteOp, register_op_impl

        @register_op_impl(op_type="{op_type}")
        class {class_name}(EagerExecuteOp):
            def execute(self, ctx):
                pass
    """).strip()
        + "\n",
        encoding="utf-8",
    )
    return file_path


class _FakeEagerContext:
    def __init__(self, inputs=None):
        self.inputs = list(inputs or [])
        self.invalidated = False
        self.stream = "fake_stream"

    @staticmethod
    def get_optional_input_tensor(ir_index):
        return ("optional", ir_index)

    def get_input_num(self):
        return len(self.inputs)

    def get_input_tensor(self, index):
        return self.inputs[index]

    def get_stream(self):
        return self.stream

    def _invalidate(self):
        self.invalidated = True


def test_register_op_impl_exports_descriptor_dict():
    @custom_op.register_op_impl(op_type="AddCustom")
    class AddCustom(custom_op.EagerExecuteOp):
        def execute(self, ctx):
            pass

    descriptors = custom_op.get_registered_op_impl_dicts()

    assert len(descriptors) == 1
    assert descriptors[0]["op_type"] == "AddCustom"
    assert descriptors[0]["class_name"] == "AddCustom"
    assert descriptors[0]["interfaces"] == ["eager_execute"]
    assert AddCustom.__ge_op_impl_descriptor__.to_bridge_dict() == descriptors[0]


def test_inputs_execute_adapts_inputs_and_injects_context_methods():
    @custom_op.register_op_impl(op_type="AutoExecuteCustom")
    class AutoExecuteCustom(custom_op.EagerExecuteOp):
        def __init__(self):
            self.seen_inputs = None
            self.seen_stream = None
            self.seen_optional = None
            self.seen_input_num = None

        def execute(self, inputs) -> None:
            self.seen_inputs = inputs
            self.seen_stream = self.get_stream()
            self.seen_optional = self.get_optional_input_tensor(1)
            self.seen_input_num = self.get_input_num()

    ctx = _FakeEagerContext(inputs=["x", "y"])
    instance = AutoExecuteCustom()

    assert instance.execute(ctx) is None
    assert instance.seen_inputs == ["x", "y"]
    assert instance.seen_stream == "fake_stream"
    assert instance.seen_optional == ("optional", 1)
    assert instance.seen_input_num == 2
    assert "get_stream" not in instance.__dict__
    assert "get_optional_input_tensor" not in instance.__dict__
    assert "get_input_num" not in instance.__dict__


def test_inputs_execute_restores_existing_instance_attribute():
    @custom_op.register_op_impl(op_type="AutoExecuteRestoreCustom")
    class AutoExecuteRestoreCustom(custom_op.EagerExecuteOp):
        def __init__(self):
            self.get_stream = "user_value"

        def execute(self, inputs) -> None:
            assert self.get_stream() == "fake_stream"

    ctx = _FakeEagerContext(inputs=["x"])
    instance = AutoExecuteRestoreCustom()

    assert instance.execute(ctx) is None
    assert instance.get_stream == "user_value"


def test_inputs_execute_ignores_return_value():
    @custom_op.register_op_impl(op_type="AutoExecuteReturnCustom")
    class AutoExecuteReturnCustom(custom_op.EagerExecuteOp):
        def execute(self, inputs) -> None:
            return True

    assert AutoExecuteReturnCustom().execute(_FakeEagerContext(inputs=["x"])) is None


def test_ctx_execute_keeps_context_argument():
    @custom_op.register_op_impl(op_type="ContextExecuteCustom")
    class ContextExecuteCustom(custom_op.EagerExecuteOp):
        def __init__(self):
            self.seen_ctx = None

        def execute(self, ctx) -> None:
            self.seen_ctx = ctx

    ctx = _FakeEagerContext(inputs=["x"])
    instance = ContextExecuteCustom()

    assert instance.execute(ctx) is None
    assert instance.seen_ctx is ctx


def test_execute_rejects_unknown_single_argument_name():
    with pytest.raises(TypeError) as exc_info:
        exec(
            textwrap.dedent("""
                class UnknownExecuteArgCustom(custom_op.EagerExecuteOp):
                    def execute(self, input) -> None:
                        pass
            """),
            {"custom_op": custom_op},
        )

    message = str(exc_info.value)
    assert (
        "Use 'ctx' for EagerOpExecutionContext or 'inputs' for the input tensor list."
        in message
    )


def test_register_op_impl_rejects_duplicate_op_type():
    @custom_op.register_op_impl(op_type="AddCustom")
    class AddCustom(custom_op.EagerExecuteOp):
        def execute(self, ctx):
            pass

    with pytest.raises(ValueError, match="op impl type already exists"):

        @custom_op.register_op_impl(op_type="AddCustom")
        class AddCustomAgain(custom_op.EagerExecuteOp):
            def execute(self, ctx):
                pass


def test_register_op_impl_rejects_invalid_op_type():
    with pytest.raises(TypeError, match="non-empty string"):
        custom_op.register_op_impl(op_type="")

    with pytest.raises(TypeError, match="non-empty string"):
        custom_op.register_op_impl(op_type=None)


def test_register_op_impl_rejects_non_custom_op_class():
    with pytest.raises(TypeError, match="expects a BaseCustomOp subclass"):

        @custom_op.register_op_impl(op_type="BadCustom")
        class BadCustom:
            pass


def test_register_op_impl_rejects_unsupported_base_only_class():
    with pytest.raises(TypeError, match="expects a supported BaseCustomOp subclass"):

        @custom_op.register_op_impl(op_type="BaseOnlyCustom")
        class BaseOnlyCustom(custom_op.BaseCustomOp):
            pass


def test_bridge_custom_op_holder_and_execute():
    @custom_op.register_op_impl(op_type="AddCustom")
    class AddCustom(custom_op.EagerExecuteOp):
        def __init__(self):
            self.called_ctx = None

        def execute(self, ctx):
            self.called_ctx = ctx
            assert ctx.stream == "fake_stream"

    descriptors = bridge.load_and_get_op_impl_descriptors()
    assert len(descriptors) == 1

    instance_id = "AddCustom#1"
    descriptor_key = descriptors[0]["descriptor_key"]
    ctx = _FakeEagerContext()
    assert bridge.create_op_impl_holder(instance_id, descriptor_key) is True
    assert bridge.call_execute(instance_id, ctx) is None
    assert ctx.invalidated is True
    assert bridge.destroy_op_impl_holder(instance_id) is True


def test_bridge_call_execute_ignores_return_value():
    @custom_op.register_op_impl(op_type="ReturnCustom")
    class ReturnCustom(custom_op.EagerExecuteOp):
        def execute(self, ctx):
            return True

    descriptor_key = bridge.load_and_get_op_impl_descriptors()[0]["descriptor_key"]
    instance_id = "ReturnCustom#1"
    ctx = _FakeEagerContext()
    assert bridge.create_op_impl_holder(instance_id, descriptor_key) is True
    assert bridge.call_execute(instance_id, ctx) is None
    assert ctx.invalidated is True
    assert bridge.destroy_op_impl_holder(instance_id) is True


def test_bridge_rejects_unknown_descriptor_key():
    with pytest.raises(KeyError, match="descriptor_key not found"):
        bridge.create_op_impl_holder("unknown#1", "not-found")


def test_bridge_loads_custom_op_plugins_from_env_path(tmp_path, monkeypatch):
    module_path = _write_custom_op_module(
        tmp_path, "env_custom_op", "EnvCustomOp", "EnvCustom"
    )
    monkeypatch.setenv(bootstrap.ENV_PY_CUSTOM_OP_PATH, str(module_path))

    descriptors = bridge.load_and_get_op_impl_descriptors()

    assert len(descriptors) == 1
    assert descriptors[0]["op_type"] == "EnvCustom"
    assert descriptors[0]["class_name"] == "EnvCustomOp"
    assert descriptors[0]["interfaces"] == ["eager_execute"]


def test_load_custom_op_plugins_rejects_missing_path(monkeypatch):
    monkeypatch.setenv(bootstrap.ENV_PY_CUSTOM_OP_PATH, "/path/not/exist/custom_op.py")

    with pytest.raises(FileNotFoundError, match="python custom op path does not exist"):
        bridge.load_and_get_op_impl_descriptors()
