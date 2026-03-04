#!/bin/bash

# Ensure we're called with a feature name
if [ -z "$1" ]; then
    echo "Usage: $0 <feature-name>"
    echo ""
    echo "Merged task branches available for cleanup:"
    git branch --merged | grep 'task/' | sed 's|*||g' | awk '{print "  " $1}' | sed 's|task/||g'
    exit 1
fi

FEATURE_NAME="$1"
WORKTREE_PATH="../review-area/review-$FEATURE_NAME"
BRANCH_NAME="task/$FEATURE_NAME"

echo "Cleaning up worktree at $WORKTREE_PATH..."
if [ -d "$WORKTREE_PATH" ]; then
    git worktree remove "$WORKTREE_PATH"
else
    echo "Worktree $WORKTREE_PATH not found, skipping."
fi

echo "Deleting branch $BRANCH_NAME..."
if git show-ref --verify --quiet "refs/heads/$BRANCH_NAME"; then
    if ! git branch --merged | grep -E -q "^\s*\*?\s*${BRANCH_NAME}$"; then
        read -p "Branch $BRANCH_NAME is not merged. Are you sure you want to delete it? [y/N] " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            echo "Skipping branch deletion."
            exit 0
        fi
        git branch -D "$BRANCH_NAME"
    else
        git branch -d "$BRANCH_NAME"
    fi
else
    echo "Branch $BRANCH_NAME not found, skipping."
fi

echo "Pruning worktrees..."
git worktree prune

echo "Cleanup complete."
