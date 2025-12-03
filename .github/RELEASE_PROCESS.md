# Release Process for chan_moq.so

This document describes how to create releases for the MoQ Channel Driver.

## Automated Release Process

The CI/CD pipeline automatically builds and releases `chan_moq.so` when you push a version tag.

### Creating a Release

1. **Ensure your code is ready**:
   ```bash
   # Make sure everything builds locally
   make clean
   make
   
   # Run any tests
   make test
   ```

2. **Update version information** (if applicable):
   - Update version numbers in documentation
   - Update CHANGELOG or release notes

3. **Commit your changes**:
   ```bash
   git add .
   git commit -m "Prepare for release v1.0.0"
   git push origin main
   ```

4. **Create and push a version tag**:
   ```bash
   # Create a tag (use semantic versioning)
   git tag -a v1.0.0 -m "Release version 1.0.0"
   
   # Push the tag to GitHub
   git push origin v1.0.0
   ```

5. **Wait for the build**:
   - Go to the "Actions" tab on GitHub
   - Watch the build progress
   - The release will be created automatically when the build succeeds

### What Gets Built

The CI/CD pipeline builds `chan_moq.so` for:
- Ubuntu 20.04 LTS
- Ubuntu 22.04 LTS

Each release includes:
- `chan_moq-ubuntu20.04.tar.gz` - Complete package for Ubuntu 20.04
- `chan_moq-ubuntu22.04.tar.gz` - Complete package for Ubuntu 22.04
- Individual `.so` files for direct download
- Configuration files (`moq.conf`)
- Documentation (`README.md`, `INSTALL.md`)
- Build information

## Manual Release Process

If you need to create a release manually:

1. **Build locally**:
   ```bash
   make clean
   make
   ```

2. **Create release package**:
   ```bash
   mkdir -p release
   cp chan_moq.so release/
   cp moq.conf release/
   cp README.md release/
   cp INSTALL.md release/
   
   tar -czf chan_moq-manual-release.tar.gz -C release .
   ```

3. **Create GitHub release manually**:
   - Go to Releases page on GitHub
   - Click "Draft a new release"
   - Choose a tag or create new one
   - Upload your tarball
   - Write release notes
   - Publish release

## Versioning Scheme

We use [Semantic Versioning](https://semver.org/):

- **MAJOR.MINOR.PATCH** (e.g., 1.2.3)
  - **MAJOR**: Incompatible API changes
  - **MINOR**: New functionality (backwards compatible)
  - **PATCH**: Bug fixes (backwards compatible)

### Version Tag Examples

- `v1.0.0` - First stable release
- `v1.1.0` - New features added
- `v1.1.1` - Bug fix
- `v2.0.0` - Breaking changes
- `v1.0.0-beta.1` - Beta release (marked as pre-release)
- `v1.0.0-rc.1` - Release candidate

## Pre-releases

Tags containing `alpha`, `beta`, or `rc` are automatically marked as pre-releases:

```bash
git tag -a v1.0.0-beta.1 -m "Beta release for testing"
git push origin v1.0.0-beta.1
```

## Continuous Integration

Every push to any branch triggers a CI build to verify the code compiles. This helps catch build issues early.

## Build Artifacts

For every push to main/master/develop branches, build artifacts are stored for 30 days and can be downloaded from the Actions tab.

## Troubleshooting Releases

### Build fails on GitHub but works locally

- Check that all dependencies are listed in the workflow
- Verify paths are correct (no hardcoded local paths)
- Check the Actions logs for specific errors

### Release not created after pushing tag

- Ensure tag starts with 'v' (e.g., `v1.0.0`)
- Check Actions tab for build status
- Verify GitHub Actions has write permissions

### Missing files in release

- Check the `release` job in `.github/workflows/build.yml`
- Ensure files are being copied to the `dist` directory
- Verify artifact upload configuration

## Security Considerations

- Never commit secrets or credentials
- Use GitHub Secrets for sensitive data
- Review all dependencies before releasing
- Test releases in a staging environment

## Release Checklist

Before creating a release:

- [ ] Code compiles without errors
- [ ] All tests pass
- [ ] Documentation is updated
- [ ] CHANGELOG is updated (if exists)
- [ ] Version numbers are updated
- [ ] Security vulnerabilities addressed
- [ ] Backward compatibility considered
- [ ] Release notes prepared

## After Release

1. **Announce the release**:
   - Update project website
   - Post on relevant forums/channels
   - Notify users

2. **Monitor for issues**:
   - Watch for bug reports
   - Monitor GitHub issues
   - Be ready to release hotfixes

3. **Plan next release**:
   - Create milestone for next version
   - Start planning new features

## Getting Help

If you encounter issues with the release process:
- Check the Actions logs on GitHub
- Review this documentation
- Open an issue with the `ci-cd` label
