---
description: Cleanup of task branch and review worktree
---

# Finish Task Workflow

This workflow handles the safe cleanup of a development branch and its corresponding review worktree.

## 1. Parameters
- `{{FEATURE_NAME}}`: The name used in the `task` workflow.

## 2. Preparation
Switch to a stable branch (e.g., `master`) before attempting to delete the task branch.

**Command**:
```bash
// turbo
git checkout master
```

## 3. Verification of Merge Status
The agent MUST check if `task/{{FEATURE_NAME}}` has been merged into `main`.

**Command**:
```bash
git branch --merged master | grep "task/{{FEATURE_NAME}}"
```

- **If merged**: Proceed to cleanup.
- **If NOT merged**: STOP and notify the user. 
    - **Prompt**: "The branch `task/{{FEATURE_NAME}}` has not been merged into `main`. Proceed with deletion anyway? This will result in loss of unmerged work."
    - **Wait** for user confirmation before executing Step 4.

## 4. Cleanup Operations

### Step 4a: Remove Worktree
**Command**:
```bash
// turbo
git worktree remove ../review-area/review-{{FEATURE_NAME}}
```

### Step 4b: Delete Branch
- If the branch was merged:
```bash
// turbo
git branch -d task/{{FEATURE_NAME}}
```

- If the branch was NOT merged (but confirmed by user):
```bash
// turbo
git branch -D task/{{FEATURE_NAME}}
```