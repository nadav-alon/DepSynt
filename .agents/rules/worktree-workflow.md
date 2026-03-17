---
trigger: model_decision
description: Best practices for working within isolated Git worktrees
---

This rule governs the behavior of an agent when executing tasks inside a dedicated worktree instance.
## 1. Context Verification
Before making any changes, ensure you are in the correct worktree and branch:
- Verify the current directory is within the correct worktree path (e.g., `../review-area/review-{{FEATURE_NAME}}`).
- Confirm the active branch matches the task branch (e.g., `task/{{FEATURE_NAME}}`).
- Use `git status` to check for leftover uncommitted or untracked changes from previous sessions.
## 2. Development Discipline
- **Atomic Commits**: Split your work into logical sections. Avoid single "megacommits." For example, separate refactors, utility additions, and core logic into different commits.
- **Meaningful Commit Messages**: Use descriptive messages that explain *why* a change was made, not just *what* was changed.
- **Incremental Progress**: Commit frequently to provide a clear audit trail. This is critical for debugging if a regression is introduced.
## 3. Workflow Integration
- The [/task](file:///home/cowclaw/DepSynt-1/.agents/workflows/task.md) workflow is used to set up the environment.
- The [/finish-task](file:///home/cowclaw/DepSynt-1/.agents/workflows/finish-task.md) workflow is used once the task is merged to clean up the workspace.
## 4. Quality & Hygiene
- **Test in Worktree**: Always verify that the project builds and tests pass *specifically* within the worktree environment.
- **Cleanup**: Identify and manage any temporary files or untracked artifacts before signaling task completion.
