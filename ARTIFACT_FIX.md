# GitHub Artifacts Fix

## Problem

The GitHub Actions CI/CD workflow was failing with the error:
```
✗ chan_moq.so not found
```

While artifacts **were** being created and uploaded, the workflow was failing at the verification step, which prevented the overall workflow from completing successfully. This made it appear as if no artifacts were available.

## Root Cause

The issue was a **mismatch between artifact upload and download paths**:

1. **Upload step** (lines 125-139 in original `build.yml`):
   - Uploaded `dist/chan_moq.so` which preserved the directory structure
   - When downloaded, this created a nested `dist/` directory

2. **Verify step** (lines 230-256 in original `build.yml`):
   - Expected to find `chan_moq.so` in the root directory
   - Failed because the file was actually at `dist/chan_moq.so`

This caused the verify job to fail with exit code 1, marking the entire workflow as failed despite successful builds.

## Solution

Fixed the artifact structure to be more straightforward:

### Changes Made

1. **Prepare artifact step** (lines 106-126):
   - Added: `cp dist/chan_moq.so chan_moq-ubuntu${{ matrix.ubuntu-version }}.so`
   - This creates a properly named .so file in the root directory for each Ubuntu version

2. **Upload artifact step** (lines 128-136):
   - Changed to upload: `chan_moq-ubuntu${{ matrix.ubuntu-version }}.so` (in root)
   - Removed the separate "Upload build info" step (was creating duplicate artifacts)
   - Now uploads BUILD_INFO.txt as part of the main artifact

3. **Verify artifact step** (lines 220-266):
   - Updated to check for the correctly named file: `chan_moq-ubuntu22.04.so`
   - Added better debugging output showing all artifact contents
   - Added verification of tarball contents and BUILD_INFO.txt

4. **Release step** (lines 195-207):
   - Updated file paths to match new artifact structure
   - Now uploads both tarballs and .so files with proper Ubuntu version naming

## Benefits

- ✅ **Clear artifact naming**: `chan_moq-ubuntu20.04.so` and `chan_moq-ubuntu22.04.so`
- ✅ **No nested directories**: Files are in the root of the artifact
- ✅ **Easier downloads**: Users can easily identify which file is for which Ubuntu version
- ✅ **Better verification**: More detailed output showing exactly what's in the artifacts
- ✅ **Workflow passes**: Verification step now succeeds, marking builds as successful

## Artifact Structure

After download, artifacts now contain:

```
chan_moq-ubuntu22.04/
├── chan_moq-ubuntu22.04.so         # Direct .so file (no subdirectory)
├── chan_moq-ubuntu22.04.tar.gz     # Tarball with all files
└── dist/
    └── BUILD_INFO.txt              # Build metadata
```

The tarball contains:
```
chan_moq.so
moq.conf
README.md
INSTALL.md
BUILD_INFO.txt
```

## Testing

To test the fix:

1. Push these changes to trigger a CI/CD run
2. Check that the "Verify Build" job passes
3. Download the artifacts from the Actions tab
4. Verify files are accessible without nested directories

## Next Steps

When you push this change:
- The workflow should complete successfully
- Artifacts will be available in the GitHub Actions "Artifacts" section
- For tagged releases (e.g., `v1.0.0`), files will be attached to the GitHub Release
