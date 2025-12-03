# GitHub Badges for README

Copy and paste these badges into your README.md. Replace `YOUR_USERNAME` and `YOUR_REPO` with your actual GitHub username and repository name.

## Basic Badges

### Build Status

```markdown
![Build and Release](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/build.yml/badge.svg)
```

### CI Status

```markdown
![CI](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/ci.yml/badge.svg)
```

## Release Badges

### Latest Release

```markdown
[![Release](https://img.shields.io/github/v/release/YOUR_USERNAME/YOUR_REPO)](https://github.com/YOUR_USERNAME/YOUR_REPO/releases)
```

### Latest Release (include pre-releases)

```markdown
[![Release](https://img.shields.io/github/v/release/YOUR_USERNAME/YOUR_REPO?include_prereleases)](https://github.com/YOUR_USERNAME/YOUR_REPO/releases)
```

### Download Count

```markdown
[![Downloads](https://img.shields.io/github/downloads/YOUR_USERNAME/YOUR_REPO/total)](https://github.com/YOUR_USERNAME/YOUR_REPO/releases)
```

## Repository Badges

### License

```markdown
[![License](https://img.shields.io/github/license/YOUR_USERNAME/YOUR_REPO)](LICENSE)
```

### Stars

```markdown
[![Stars](https://img.shields.io/github/stars/YOUR_USERNAME/YOUR_REPO)](https://github.com/YOUR_USERNAME/YOUR_REPO/stargazers)
```

### Issues

```markdown
[![Issues](https://img.shields.io/github/issues/YOUR_USERNAME/YOUR_REPO)](https://github.com/YOUR_USERNAME/YOUR_REPO/issues)
```

### Last Commit

```markdown
[![Last Commit](https://img.shields.io/github/last-commit/YOUR_USERNAME/YOUR_REPO)](https://github.com/YOUR_USERNAME/YOUR_REPO/commits)
```

## Combined Badge Section

Here's a complete badge section you can add to your README:

```markdown
# MoQ Channel Driver for Asterisk

![Build](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/build.yml/badge.svg)
![CI](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/ci.yml/badge.svg)
[![Release](https://img.shields.io/github/v/release/YOUR_USERNAME/YOUR_REPO)](https://github.com/YOUR_USERNAME/YOUR_REPO/releases)
[![Downloads](https://img.shields.io/github/downloads/YOUR_USERNAME/YOUR_REPO/total)](https://github.com/YOUR_USERNAME/YOUR_REPO/releases)
[![License](https://img.shields.io/github/license/YOUR_USERNAME/YOUR_REPO)](LICENSE)
[![Stars](https://img.shields.io/github/stars/YOUR_USERNAME/YOUR_REPO)](https://github.com/YOUR_USERNAME/YOUR_REPO/stargazers)

A channel driver for Asterisk that implements Media over QUIC (MoQ) protocol...
```

## Alternative: With Shields.io

### Build Status (Alternative)

```markdown
![Build](https://img.shields.io/github/actions/workflow/status/YOUR_USERNAME/YOUR_REPO/build.yml?branch=main&label=build)
```

### Custom Styled Badge

```markdown
[![chan_moq.so](https://img.shields.io/badge/chan__moq.so-Download-blue?style=for-the-badge&logo=github)](https://github.com/YOUR_USERNAME/YOUR_REPO/releases)
```

## Platform-Specific Badges

### Ubuntu Support

```markdown
![Ubuntu 20.04](https://img.shields.io/badge/Ubuntu-20.04-E95420?logo=ubuntu)
![Ubuntu 22.04](https://img.shields.io/badge/Ubuntu-22.04-E95420?logo=ubuntu)
```

### Language/Tech

```markdown
![C](https://img.shields.io/badge/Language-C-A8B9CC?logo=c)
![Asterisk](https://img.shields.io/badge/Asterisk-Compatible-orange)
```

## Badge Styles

Shields.io supports different styles. Add `?style=STYLE` to the URL:

- `flat` (default)
- `flat-square`
- `plastic`
- `for-the-badge`
- `social`

Example:
```markdown
![Build](https://img.shields.io/github/actions/workflow/status/YOUR_USERNAME/YOUR_REPO/build.yml?style=for-the-badge)
```

## How to Use

1. Copy the badge markdown
2. Replace `YOUR_USERNAME` with your GitHub username
3. Replace `YOUR_REPO` with your repository name
4. Paste into your README.md at the desired location
5. Commit and push

## Example Result

After adding badges, your README will show:

- ✅ Green checkmark when builds pass
- ❌ Red X when builds fail
- 🔵 Blue badge for latest release version
- 📊 Download statistics
- ⭐ Star count

These badges update automatically!
