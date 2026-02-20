# Workflow Dispatch Not Showing in GitHub Actions UI

## Problem

The "Run workflow" button doesn't appear in GitHub Actions UI because `workflow_dispatch` only works for workflows on the **default branch** or the specific branch you're viewing.

## Current Situation

- **Default branch:** `master`
- **Workflow location:** `spark` branch
- **Issue:** GitHub Actions UI only shows workflow_dispatch for workflows on the default branch

## Solutions

### Option 1: Merge to Master (Recommended)

Merge the `spark` branch into `master` so the workflow is available on the default branch:

```bash
# Switch to master branch
git checkout master

# Merge spark branch
git merge spark

# Push to remote
git push origin master
```

After this, you'll see the "Run workflow" button in GitHub Actions UI.

### Option 2: Change Repository Default Branch to 'spark'

Change the default branch in GitHub repository settings:

1. Go to: `https://github.com/wangyum/jdk25u-dev/settings/branches`
2. Under "Default branch", click the switch icon
3. Select `spark` as the new default branch
4. Confirm the change

**Note:** This affects the entire repository, not just workflows.

### Option 3: Use GitHub CLI Instead of UI

You can trigger workflow_dispatch from any branch using GitHub CLI:

```bash
# Trigger workflow on spark branch
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch" \
  -f query_filter="q3" \
  -f log_level="ERROR"
```

This works immediately, no merge needed!

### Option 4: Use GitHub API

Trigger via curl/API:

```bash
curl -X POST \
  -H "Accept: application/vnd.github.v3+json" \
  -H "Authorization: token YOUR_GITHUB_TOKEN" \
  https://api.github.com/repos/wangyum/jdk25u-dev/actions/workflows/tpcds-benchmark.yml/dispatches \
  -d '{
    "ref": "spark",
    "inputs": {
      "baseline_opts": "-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch",
      "optimized_opts": "-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch",
      "query_filter": "q3",
      "log_level": "ERROR"
    }
  }'
```

## Verification

After merging to master (Option 1) or changing default branch (Option 2):

1. Go to: `https://github.com/wangyum/jdk25u-dev/actions`
2. Click on "TPC-DS Benchmark with JDK 25" in the left sidebar
3. You should see a "Run workflow" dropdown button on the right
4. Click it to see the input fields

## Why This Happens

GitHub's workflow_dispatch feature only shows in the UI for workflows that exist on the default branch. This is a security measure to prevent arbitrary workflow execution from feature branches.

From GitHub's documentation:
> "The workflow_dispatch event is only available for workflows on the default branch."

However, you can **trigger** workflow_dispatch on any branch using:
- GitHub CLI (`gh workflow run`)
- GitHub API
- The UI (if workflow exists on default branch, you can select which branch to run it on)

## Recommended Approach

**For immediate use:**
```bash
# Use GitHub CLI to trigger on spark branch
gh workflow run tpcds-benchmark.yml --ref spark -f query_filter="q3"
```

**For long-term UI access:**
```bash
# Merge to master
git checkout master
git merge spark
git push origin master
```

Then you can use the GitHub Actions UI to trigger workflows on any branch.

## Testing the Fix

### Using GitHub CLI

```bash
# Quick test with default values
gh workflow run tpcds-benchmark.yml --ref spark

# Quick test with single query
gh workflow run tpcds-benchmark.yml --ref spark -f query_filter="q3"

# Custom configuration
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch" \
  -f query_filter="q3,q7,q19" \
  -f log_level="ERROR"
```

### Check Workflow Run

```bash
# List recent workflow runs
gh run list --workflow=tpcds-benchmark.yml

# Watch a specific run
gh run watch <run-id>

# View run logs
gh run view <run-id> --log
```

## Summary

**Quick fix (works now):**
```bash
gh workflow run tpcds-benchmark.yml --ref spark -f query_filter="q3"
```

**Permanent fix (for UI access):**
```bash
git checkout master
git merge spark
git push origin master
```

After the permanent fix, you can use both:
- GitHub Actions UI (browser)
- GitHub CLI (`gh` command)
- GitHub API

All three methods will work!
