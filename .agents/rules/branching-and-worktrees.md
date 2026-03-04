---
trigger: always_on
---

# Branching and Worktrees Methodology

When working on separate tasks, adhere to the following workflow to ensure tasks are isolated and can be reviewed independently without interrupting the agent's workflow:

1. **Branch Creation**: Always create a separate branch for each distinct task or feature before making any codebase changes.
   - Use descriptive branch names indicating the task.
   - `git checkout -b task/your-feature-name`

2. **Commits*: Make sure to seperate work to different commits when it makes sense, so the flow of the work and repo history can be logically inferred. No need for multiple commits for small changes and fixes, but for a big feature that has a bunch of different working parts that might require debugging on which commit broke something, multiple commits can help.

3. **Worktrees for Review**: To allow the user to review the code while the agent is still coding or iterating on feedback, create a git worktree in a separate directory outside the main repository directory. To do this, you must checkout the previous branch first before creating the worktree.
   - `git checkout <previous-branch>`
   - `git worktree add ../review-area/review-<feature-name> task/your-feature-name`
   - `code ../review-area/review-<feature-name>` to open vscode automatically on that branch, to allow the user to inspect the state, run tests, or review changes without interfering with the ongoing development and open files in the main repository.

4. **Cleanup**: Once the task is reviewed, accepted, and merged, remember to remove the worktree and the branch.
   - `git worktree remove ../review-<feature-name>`
   - `git branch -d task/your-feature-name`
