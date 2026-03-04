#compdef cleanup-worktree.sh

_cleanup_worktree() {
    local -a branches
    branches=("${(@f)$(git branch --list 'task/*' --format='%(refname:short)' | sed 's|^task/||')}")
    _describe 'feature' branches
}

compdef _cleanup_worktree cleanup-worktree.sh
