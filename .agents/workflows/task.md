---
description: Branching and Worktrees Methodology for task isolation
---

This workflow ensures that tasks are isolated and can be worked on independently.

Upon getting a task from the user, first generate a name for the feature. Then create a dedicated branch and an accompanying review worktree.

- **Branch Naming**: Use `task/your-feature-name`.
- **Review Strategy**: The worktree is created immediately, work in it will be done in an additional agent session.

**Commands**:
```bash
git branch task/{{FEATURE_NAME}}
mkdir -p ../review-area
git worktree add ../review-area/review-{{FEATURE_NAME}} task/{{FEATURE_NAME}}
```
*(Replace `{{FEATURE_NAME}}` with the actual feature name).*

**IMPORTANT** after this workflow, do not start working on the problem. This is only a creation of a starting ground on which a different agent will start working on it.