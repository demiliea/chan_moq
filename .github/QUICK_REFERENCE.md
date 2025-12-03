# CI/CD Quick Reference Card

## 🚀 Essential Commands

### Create a Release

```bash
git tag -a v1.0.0 -m "Release 1.0.0"
git push origin v1.0.0
```

### Test CI Build

```bash
git push origin main  # Any push triggers CI
```

### Check Setup

```bash
./check-cicd.sh
```

### Download Latest Release (for users)

```bash
wget https://github.com/USER/REPO/releases/latest/download/chan_moq-ubuntu22.04.tar.gz
tar -xzf chan_moq-ubuntu22.04.tar.gz
```

## 📁 File Structure

```
.github/
├── workflows/
│   ├── build.yml          # Main build & release
│   └── ci.yml            # Quick CI checks
├── README.md             # CI/CD overview
├── SETUP.md              # Setup guide
├── RELEASE_PROCESS.md    # How to release
├── BADGES.md             # Status badges
└── QUICK_REFERENCE.md    # This file

check-cicd.sh             # Validation script
```

## 🔄 Workflow Triggers

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `build.yml` | main/master/develop push, tags `v*` | Full build & release |
| `ci.yml` | Any branch push, all PRs | Quick validation |

## 📦 What Gets Built

- `chan_moq.so` (Ubuntu 20.04)
- `chan_moq.so` (Ubuntu 22.04)
- `chan_moq-ubuntu20.04.tar.gz`
- `chan_moq-ubuntu22.04.tar.gz`
- Configuration files
- Documentation

## ⚙️ Required GitHub Settings

**Settings → Actions → General → Workflow permissions:**
- ✅ Read and write permissions
- ✅ Allow GitHub Actions to create and approve pull requests

## 🏷️ Version Tag Format

✅ **Correct:**
- `v1.0.0`
- `v2.1.3`
- `v1.0.0-beta.1`
- `v2.0.0-rc.1`

❌ **Wrong:**
- `1.0.0` (missing 'v')
- `release-1.0` (wrong format)
- `ver1.0.0` (wrong prefix)

## 📊 Status Badges

```markdown
![Build](https://github.com/USER/REPO/actions/workflows/build.yml/badge.svg)
![CI](https://github.com/USER/REPO/actions/workflows/ci.yml/badge.svg)
[![Release](https://img.shields.io/github/v/release/USER/REPO)](https://github.com/USER/REPO/releases)
```

## 🎯 Common Tasks

### Check Build Status
→ Go to **Actions** tab on GitHub

### Download Build Artifacts
→ Actions → Select run → Artifacts section

### View Releases
→ **Releases** section (right sidebar)

### Manual Trigger
→ Actions → Select workflow → Run workflow

## 🔍 Troubleshooting

| Problem | Solution |
|---------|----------|
| Build fails | Check Actions logs |
| No release created | Verify tag format (v*), check permissions |
| Can't find artifacts | Actions → workflow run → Artifacts section |
| Push doesn't trigger CI | Check `.github/workflows/` files exist |

## ⏱️ Build Times

- **CI Check**: ~5-10 minutes
- **Full Build**: ~10-20 minutes  
- **Release**: ~15-25 minutes

## 📝 Release Checklist

- [ ] Code compiles locally
- [ ] All tests pass
- [ ] Documentation updated
- [ ] Version numbers updated
- [ ] Committed and pushed to main
- [ ] Create version tag
- [ ] Push tag to GitHub
- [ ] Verify release created
- [ ] Test download links

## 🔗 Important Links

| Resource | Location |
|----------|----------|
| Workflow runs | `github.com/USER/REPO/actions` |
| Releases | `github.com/USER/REPO/releases` |
| Latest release | `github.com/USER/REPO/releases/latest` |
| CI badge | `github.com/USER/REPO/actions/workflows/ci.yml/badge.svg` |

## 💡 Tips

- **Tag naming**: Always prefix with 'v' (e.g., `v1.0.0`)
- **Semantic versioning**: MAJOR.MINOR.PATCH
- **Pre-releases**: Use `-beta`, `-rc`, `-alpha` suffix
- **Artifacts**: Kept for 30 days, releases forever
- **Caching**: CI workflow caches dependencies for faster builds

## 🆘 Need Help?

1. Check workflow logs in Actions tab
2. Run `./check-cicd.sh`
3. Read `.github/README.md`
4. Review `.github/SETUP.md`

## 📖 Documentation

| File | Purpose |
|------|---------|
| `CICD_SETUP_COMPLETE.md` | Setup completion guide |
| `.github/README.md` | Comprehensive CI/CD docs |
| `.github/SETUP.md` | Step-by-step setup |
| `.github/RELEASE_PROCESS.md` | Release instructions |
| `.github/BADGES.md` | Status badge templates |

---

**Remember**: Every push tests your code. Every tag creates a release. 🚀
