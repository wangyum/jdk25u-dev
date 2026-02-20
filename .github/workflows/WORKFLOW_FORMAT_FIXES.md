# Workflow Format Fixes

## Issues Found and Fixed

### 1. ❌ YAML Syntax Error (CRITICAL)

**Problem:**
```
yaml.scanner.ScannerError: while scanning a simple key
  in ".github/workflows/spark-benchmark.yml", line 337, column 1
could not find expected ':'
```

The embedded Python script (124 lines) inside a heredoc was causing YAML parsing issues. Complex multi-line heredoc content with special characters conflicted with YAML syntax.

**Root Cause:**
- Heredoc with 124 lines of Python code inline in YAML `run:` block
- YAML parser struggled with embedded Python syntax (colons, quotes, f-strings)
- Made the workflow file bloated (461 lines) and hard to maintain

**Fix:**
- Extracted Python comparison script to separate file: `.github/scripts/compare_results.py`
- Updated workflow to checkout code and run external script
- Added sparse checkout to only fetch the script directory

**Before (Lines 336-445):**
```yaml
- name: Analyze and compare benchmark results
  run: |
    cat > compare_results.py <<'PYTHON'
    import re
    import sys
    import os
    # ... 124 lines of Python code ...
    PYTHON
    python3 compare_results.py | tee comparison_output.txt
```

**After (Lines 319-343):**
```yaml
- name: Checkout code for comparison script
  uses: actions/checkout@v4
  with:
    sparse-checkout: |
      .github/scripts
    sparse-checkout-cone-mode: false

- name: Analyze and compare benchmark results
  run: |
    python3 .github/scripts/compare_results.py | tee comparison_output.txt
```

### 2. ✅ Missing Python Import

**Problem:**
The embedded Python script used `os.path.exists()` but didn't import `os`.

**Fix:**
Added `import os` to the extracted script.

## Benefits of Refactoring

### 1. Cleaner Workflow ✅
- **Before:** 461 lines with embedded Python
- **After:** 356 lines (23% reduction)
- Easier to read and maintain

### 2. Valid YAML ✅
- No syntax errors
- Passes `yaml.safe_load()` validation
- GitHub Actions will parse it correctly

### 3. Better Separation of Concerns ✅
- **Workflow:** Orchestrates jobs and steps
- **Python script:** Handles comparison logic
- Each file has a single responsibility

### 4. Easier Testing ✅
```bash
# Can now test the Python script locally
python3 .github/scripts/compare_results.py
```

### 5. Reusability ✅
- Script can be used in other workflows
- Can be run locally for debugging
- Version controlled separately

### 6. Better Error Messages ✅
- Python errors now show clear file and line numbers
- Not buried in heredoc context

## Files Changed

### Created:
- `.github/scripts/compare_results.py` - Python comparison script (119 lines)

### Modified:
- `.github/workflows/spark-benchmark.yml` - Removed heredoc, use external script (356 lines)

## Validation

```bash
# YAML syntax validation
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/spark-benchmark.yml'))"
# ✅ YAML syntax is valid

# Python script validation
python3 -m py_compile .github/scripts/compare_results.py
# ✅ No syntax errors
```

## Final Structure

```
.github/
├── scripts/
│   └── compare_results.py          # Comparison logic
└── workflows/
    └── spark-benchmark.yml          # Workflow orchestration (356 lines)
```

## Summary

**Problem:** YAML syntax error due to 124-line Python heredoc  
**Solution:** Extract to external script in `.github/scripts/`  
**Result:** Clean, valid YAML with proper separation of concerns

✅ YAML syntax is valid  
✅ File size reduced by 23%  
✅ Missing import fixed  
✅ Easier to maintain and test  
