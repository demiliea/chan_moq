# GitHub Actions CI/CD

This directory contains the GitHub Actions workflows for building and releasing the MoQ Channel Driver.

## Workflows

### 1. Build and Release (`build.yml`)

**Trigger**: 
- Push to `main`, `master`, or `develop` branches
- Pull requests to these branches
- Version tags (e.g., `v1.0.0`)
- Manual dispatch

**What it does**:
- Builds `chan_moq.so` for Ubuntu 20.04 and 22.04
- Runs basic tests
- Creates release artifacts
- For version tags: Creates GitHub Release with binaries

**Matrix Build**:
- Ubuntu 20.04 LTS
- Ubuntu 22.04 LTS

### 2. Quick CI Check (`ci.yml`)

**Trigger**:
- Every push to any branch
- All pull requests

**What it does**:
- Quick compilation check
- Dependency verification
- Basic static analysis
- Faster feedback for development

## Using the CI/CD Pipeline

### For Development

Push your code to any branch:
```bash
git push origin feature/my-feature
```

The CI will automatically:
1. Check that the code compiles
2. Verify dependencies
3. Report results in the PR/commit status

### Creating a Release

To create a new release with binaries:

```bash
# Create and push a version tag
git tag -a v1.0.0 -m "Release 1.0.0"
git push origin v1.0.0
```

The pipeline will:
1. Build `chan_moq.so` for multiple Ubuntu versions
2. Run tests
3. Create a GitHub Release
4. Upload compiled binaries and packages
5. Include release notes

See [RELEASE_PROCESS.md](RELEASE_PROCESS.md) for detailed instructions.

## Build Artifacts

After each build, artifacts are available:
- Go to Actions tab → Select workflow run → Scroll to Artifacts section
- Download `chan_moq-ubuntu20.04` or `chan_moq-ubuntu22.04`
- Artifacts are kept for 30 days

## Release Packages

Each release includes:
- `chan_moq-ubuntu20.04.tar.gz` - Full package for Ubuntu 20.04
- `chan_moq-ubuntu22.04.tar.gz` - Full package for Ubuntu 22.04
- Individual `chan_moq.so` files
- Configuration files
- Documentation
- Build information

## Dependencies

The workflows automatically install:
- GCC and build tools
- libwebsockets-dev
- libjson-c-dev
- Asterisk development headers

## Badges

Add these badges to your README.md:

```markdown
![Build Status](https://github.com/YOUR_USERNAME/YOUR_REPO/workflows/Build%20and%20Release%20chan_moq.so/badge.svg)
![CI](https://github.com/YOUR_USERNAME/YOUR_REPO/workflows/CI%20-%20Quick%20Build%20Check/badge.svg)
```

Replace `YOUR_USERNAME` and `YOUR_REPO` with your GitHub username and repository name.

## Troubleshooting

### Build Fails

1. **Check the logs**: Go to Actions tab → failed workflow → View logs
2. **Common issues**:
   - Missing dependencies (workflow should install them)
   - Compilation errors (fix in your code)
   - Header path issues (verify Makefile)

### Release Not Created

1. **Verify tag format**: Must start with `v` (e.g., `v1.0.0`)
2. **Check permissions**: GitHub Actions needs write access to create releases
3. **Review logs**: Check the `release` job logs

### Artifacts Missing

1. **Check upload step**: Review the `Upload artifact` step logs
2. **Verify paths**: Ensure files exist in expected locations
3. **Check workflow file**: Verify paths in artifact upload configuration

## Security

- No secrets or credentials should be committed
- GitHub automatically provides `GITHUB_TOKEN` for releases
- Dependencies are installed from official repositories

## Customization

### Add More Platforms

Edit `.github/workflows/build.yml`:

```yaml
strategy:
  matrix:
    ubuntu-version: ['20.04', '22.04', '24.04']  # Add more versions
```

### Change Release Behavior

Edit the `release` job in `build.yml`:
- Modify `body_path` for different release notes
- Change `prerelease` logic for pre-release detection
- Add more files to upload

### Adjust CI Triggers

Edit the `on:` section:

```yaml
on:
  push:
    branches:
      - main
      - develop  # Add or remove branches
  pull_request:
    branches:
      - main
```

## Performance

- **CI builds**: ~5-10 minutes
- **Full release builds**: ~10-20 minutes
- **Caching**: Dependencies are cached to speed up builds

## Contributing

When contributing:
1. Your PR will automatically trigger a CI build
2. Wait for the build to pass before requesting review
3. Fix any compilation errors or warnings
4. Ensure the build is green ✅

## Support

For CI/CD related issues:
- Check workflow logs first
- Review this documentation
- Open an issue with the `ci-cd` label
