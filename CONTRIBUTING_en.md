# Contributing Guide

This project welcomes developers to experience and participate in contributions. Before participating in community contributions, please see [cann-community](https://gitcode.com/cann/community) to understand the code of conduct, sign the CLA agreement, and understand the contribution process of source repositories. This repository details the prerequisites for participating in CANN open source project contributions, including but not limited to:

1. How to submit Pull Requests
2. gitcode workflow
3. Pipeline trigger commands ([Online Pipeline Issue Guide](https://gitcode.com/cann/ge/wiki/%E7%BA%BF%E4%B8%8A%E6%B5%81%E6%B0%B4%E7%BA%BF%E9%97%AE%E9%A2%98%E6%8C%87%E5%AF%BC.md))
4. Code review
5. Other Precautions
   For details, please refer to [cann-community](https://gitcode.com/cann/community).

In addition, developers need to pay attention to the following points when preparing local code and submitting Pull Requests:

1. When submitting a Pull Request, please carefully fill in the business background, purpose, solution, and other information according to the Pull Request template.
2. Before committing code using git, you can refer to [pre-commit tool usage instructions](docs/zh/contributions/precommit_guide.md) to make your code submission more compliant and efficient.
3. If your modification is not a simple bug fix, but involves adding new features, new interfaces, new configuration parameters, or modifying code flows, please be sure to discuss the solution through an Issue first to avoid your code being rejected for merge. If you are unsure whether this modification can be classified as a "simple bug fix", you can also discuss the solution by submitting an Issue.
4. When submitting a Pull Request, please ensure your code complies with the project's code standards. Please refer to Google's [Open Source Code Standards](https://google.github.io/styleguide/), including but not limited to:
   - Code formatting
   - Comment standards
   - Variable naming standards
   - Function naming standards
   - Class naming standards
   - Interface naming standards
   - Configuration parameter naming standards
   - Code flow standards
5. When submitting a Pull Request, if there are multiple invalid commits, we recommend performing a rebase operation before submitting the Pull Request to merge multiple commits into one to maintain code simplicity and readability. Please refer to [git rebase](https://git-scm.com/docs/git-rebase). Also, commit messages need to comply with the project's code standards and clearly describe the intent and content of this change. The format is: <type>: <brief description>. For example:

| Type     | Description                       | Example                         |
| -------- | --------------------------------- | ------------------------------- |
| feat     | New feature                       | feat: Add user registration function |
| fix      | Bug fix                           | fix: Fix login state expiration issue |
| docs     | Documentation update              | docs: Update API usage instructions |
| style    | Code format adjustment (does not affect logic) | style: Adjust code indentation |
| refactor | Refactoring (non-feature addition/fix) | refactor: Optimize user service class structure |
| perf     | Performance optimization          | perf: Reduce database query count |
| test     | Test related                      | test: Add login function unit test |
| chore    | Build/toolchain change            | chore: Update webpack configuration |
| ci       | CI configuration related          | ci: Add automated test flow |

Developer contribution scenarios mainly include:

- Bug Fix

  If you discover certain bugs in this project and wish to fix them, you are welcome to create a new Issue for feedback and tracking.

  You can follow the [Submit Issue/Handle Issue Task](https://gitcode.com/cann/community#提交Issue处理Issue任务) guide to create a `Bug-Report|Bug Feedback` type Issue to describe the bug, then enter "/assign" or "/assign @yourself" in the comment box to assign the Issue to yourself for handling.

- Contribute New Features

  If you discover certain feature gaps in this project and wish to add them, you are welcome to create a new Issue for feedback and tracking.

  You can follow the [Submit Issue/Handle Issue Task](https://gitcode.com/cann/community#提交Issue处理Issue任务) guide to create a `Requirement|Feature Suggestion` type Issue to explain the new feature and provide your design solution,
  then enter "/assign" or "/assign @yourself" in the comment box to assign the Issue to yourself for tracking implementation.

- Documentation Correction

  If you discover certain documentation description errors in this project, you are welcome to create a new Issue for feedback and correction.

  You can follow the [Submit Issue/Handle Issue Task](https://gitcode.com/cann/community#提交Issue处理Issue任务) guide to create a `Documentation|Documentation Feedback` type Issue to point out the corresponding documentation problem, then enter "/assign" or "/assign @yourself" in the comment box to assign the Issue to yourself to correct the corresponding documentation description.

- Help Resolve Others' Issues

  If you have appropriate solutions for problems encountered by others in the community, you are welcome to comment and discuss in the Issue to help others solve problems and pain points, and jointly optimize usability.

  If the corresponding Issue requires code modification, you can enter "/assign" or "/assign @yourself" in the Issue comment box to assign the Issue to yourself to track and assist in solving the problem.
