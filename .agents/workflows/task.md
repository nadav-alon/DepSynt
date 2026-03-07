---
description: Branching and Worktrees Methodology for task isolation and review
---
# Branching and Worktrees Workflow

This workflow ensures that tasks are isolated and can be reviewed independently without interrupting ongoing development.

## 1. Task Initialization
Before making any codebase changes, create a dedicated branch and an accompanying review worktree.

- **Branch Naming**: Use `task/your-feature-name`.
- **Review Strategy**: The worktree is created immediately so the user can monitor progress in real-time.

**Commands**:
```bash
// turbo
git checkout -b task/{{FEATURE_NAME}}
// turbo
git checkout --detach
// turbo
mkdir -p ../review-area
// turbo
git worktree add ../review-area/review-{{FEATURE_NAME}} task/{{FEATURE_NAME}}
// turbo
git checkout task/{{FEATURE_NAME}}
// turbo
code ../review-area/review-{{FEATURE_NAME}}
```
*(Replace `{{FEATURE_NAME}}` with the actual feature name).*

## 2. Incremental Development
- **Coding Location**: The agent MUST switch to the **worktree directory** (`../review-area/review-{{FEATURE_NAME}}`) and perform all development there.
- **Benefit**: This allows the user to see the agent's work-in-progress files directly in the review area.
- **Commits**: Make logical commits within the worktree.
- **Large Features**: Segregate into multiple commits to facilitate debugging.


## 4. Cleanup
Once the task is reviewed, accepted, and merged into the main branch:
- **Remove Worktree**:
```bash
// turbo
git worktree remove ../review-area/review-{{FEATURE_NAME}}
```
- **Delete Branch**:
```bash
// turbo
git branch -d task/{{FEATURE_NAME}}
```
