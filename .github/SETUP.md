# CI/CD Setup Guide

This guide will help you set up the GitHub Actions CI/CD pipeline for your repository.

## Initial Setup

The CI/CD workflows are already configured! Here's what you need to do:

### 1. Push to GitHub

If you haven't already, push this repository to GitHub:

```bash
# Add GitHub as remote (if not already added)
git remote add origin https://github.com/YOUR_USERNAME/YOUR_REPO.git

# Push code and workflows
git add .
git commit -m "Add CI/CD workflows"
git push -u origin main
```

### 2. Enable GitHub Actions

GitHub Actions should be enabled by default. Verify:

1. Go to your repository on GitHub
2. Click the "Actions" tab
3. If prompted, click "I understand my workflows, go ahead and enable them"

### 3. Set Permissions (for Releases)

To allow automatic release creation:

1. Go to your repository on GitHub
2. Click "Settings" → "Actions" → "General"
3. Scroll to "Workflow permissions"
4. Select "Read and write permissions"
5. Check "Allow GitHub Actions to create and approve pull requests"
6. Click "Save"

## Testing the CI/CD

### Test Build (Without Release)

Push any code to trigger a build:

```bash
git add .
git commit -m "Test CI/CD"
git push
```

Then:
1. Go to Actions tab on GitHub
2. Watch the "CI - Quick Build Check" and "Build and Release" workflows run
3. Check that both builds complete successfully ✅

### Test Release Creation

Create and push a version tag:

```bash
# Create a test release
git tag -a v0.1.0 -m "Test release"
git push origin v0.1.0
```

Then:
1. Go to Actions tab
2. Wait for "Build and Release" workflow to complete
3. Go to "Releases" section
4. Verify the release was created with binaries

## Adding Status Badges

Add build status badges to your README.md:

```markdown
![Build](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/build.yml/badge.svg)
![CI](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/ci.yml/badge.svg)
[![Release](https://img.shields.io/github/v/release/YOUR_USERNAME/YOUR_REPO)](https://github.com/YOUR_USERNAME/YOUR_REPO/releases)
```

Replace `YOUR_USERNAME/YOUR_REPO` with your actual GitHub username and repository name.

## Workflow Overview

### 1. Quick CI Check (`ci.yml`)

**Triggers**:
- Every push to any branch
- All pull requests

**Purpose**: Fast feedback during development

**What it does**:
- Quick build verification
- Basic static analysis
- Reports build status

### 2. Full Build and Release (`build.yml`)

**Triggers**:
- Push to main/master/develop
- Pull requests to main/master/develop  
- Version tags (v*)
- Manual trigger

**Purpose**: Complete build and release process

**What it does**:
- Builds for Ubuntu 20.04 and 22.04
- Runs tests
- Creates artifacts (30-day retention)
- Creates GitHub releases (for version tags)

## Using the Pipeline

### During Development

Just push your code:
```bash
git add .
git commit -m "Add new feature"
git push
```

CI will automatically:
- Build your code
- Report success/failure
- Store artifacts for 30 days

### Creating a Release

```bash
# Ensure code is ready
make clean && make

# Commit final changes
git add .
git commit -m "Prepare release v1.0.0"
git push

# Create and push tag
git tag -a v1.0.0 -m "Release version 1.0.0"
git push origin v1.0.0
```

The release will be automatically created with:
- Compiled binaries for Ubuntu 20.04 and 22.04
- Configuration files
- Documentation
- Release notes

### Downloading Build Artifacts

For any build (not just releases):

1. Go to "Actions" tab
2. Click on a workflow run
3. Scroll to "Artifacts" section
4. Download `chan_moq-ubuntu20.04` or `chan_moq-ubuntu22.04`

## Customization

### Change Target Branches

Edit `.github/workflows/build.yml` and `.github/workflows/ci.yml`:

```yaml
on:
  push:
    branches:
      - main        # Add/remove branches
      - develop
      - production
```

### Add More Ubuntu Versions

Edit `.github/workflows/build.yml`:

```yaml
strategy:
  matrix:
    ubuntu-version: ['20.04', '22.04', '24.04']  # Add versions
```

### Adjust Artifact Retention

Edit `.github/workflows/build.yml`:

```yaml
- name: Upload artifact
  uses: actions/upload-artifact@v4
  with:
    retention-days: 90  # Change from 30 to desired days (max 90)
```

### Custom Release Notes

Edit `.github/workflows/build.yml` in the `release` job:

```yaml
- name: Create Release Notes
  run: |
    cat > RELEASE_NOTES.md << 'EOF'
    ## Your Custom Release Notes
    ...
    EOF
```

## Troubleshooting

### Build Fails with "Permission Denied"

**Solution**: Enable write permissions (see step 3 in Initial Setup)

### Release Not Created After Pushing Tag

**Possible causes**:
1. Tag doesn't start with 'v' → Use `v1.0.0` format
2. Build failed → Check Actions logs
3. Missing permissions → Enable write permissions

### Artifacts Not Uploaded

**Check**:
1. Build completed successfully
2. Files exist in expected locations
3. Workflow file paths are correct

### Different Asterisk Version Needed

Edit `.github/workflows/build.yml` and `.github/workflows/ci.yml`:

```yaml
# Change this line in both files
ASTERISK_VERSION="20.10.1"  # Update to desired version
```

## Security Best Practices

1. **Never commit secrets**: Use GitHub Secrets for sensitive data
2. **Review dependencies**: Verify all installed packages
3. **Pin versions**: Consider pinning action versions for stability
4. **Monitor builds**: Watch for suspicious activity

## Advanced Configuration

### Using GitHub Secrets

If you need to use secrets (API keys, credentials):

1. Go to Settings → Secrets and variables → Actions
2. Click "New repository secret"
3. Add your secret
4. Reference in workflow:

```yaml
- name: Step using secret
  env:
    MY_SECRET: ${{ secrets.MY_SECRET }}
  run: |
    # Use $MY_SECRET here
```

### Caching Dependencies

Already implemented in `ci.yml` for faster builds:

```yaml
- name: Cache dependencies
  uses: actions/cache@v3
  with:
    path: |
      /usr/include/asterisk
    key: ${{ runner.os }}-deps-${{ hashFiles('Makefile') }}
```

### Notifications

To get notified of build failures:

1. Go to your repository on GitHub
2. Click "Watch" → "Custom" → "Actions"
3. Or use [GitHub Mobile app](https://github.com/mobile)

## Getting Help

- Check [GitHub Actions documentation](https://docs.github.com/en/actions)
- Review workflow logs in Actions tab
- Check `.github/README.md` for workflow details
- Open an issue with `ci-cd` label

## Checklist

After setup:

- [ ] Repository pushed to GitHub
- [ ] GitHub Actions enabled
- [ ] Write permissions enabled for releases
- [ ] Test build completed successfully
- [ ] Test release created successfully
- [ ] Status badges added to README
- [ ] Team notified about CI/CD setup

## Next Steps

1. **Test the pipeline**: Push code and create a test release
2. **Add badges**: Include status badges in your README
3. **Document versioning**: Decide on versioning scheme
4. **Set up notifications**: Configure build notifications
5. **Train team**: Share this documentation with contributors

Congratulations! Your CI/CD pipeline is ready to use! 🎉
