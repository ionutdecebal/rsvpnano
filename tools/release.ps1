#!/usr/bin/env pwsh
# Usage: .\tools\release.ps1 v0.0.7-noogi [--branch release/rsvp-noogi]
# Creates a git tag and pushes it to origin, which triggers the GitHub Actions release workflow.

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$Version,

    [string]$Branch = "release/rsvp-noogi"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Validate version format
if ($Version -notmatch '^v\d+\.\d+\.\d+') {
    Write-Error "Version must start with 'v' followed by semver, e.g. v0.0.7-noogi"
    exit 1
}

# Check git is available
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Error "git not found in PATH"
    exit 1
}

# Check current branch
$currentBranch = git rev-parse --abbrev-ref HEAD
if ($currentBranch -ne $Branch) {
    Write-Host "Current branch: $currentBranch" -ForegroundColor Yellow
    Write-Host "Switching to $Branch ..." -ForegroundColor Cyan
    git checkout $Branch
    if ($LASTEXITCODE -ne 0) { exit 1 }
}

# Pull latest
Write-Host "Pulling latest $Branch ..." -ForegroundColor Cyan
git pull origin $Branch
if ($LASTEXITCODE -ne 0) { exit 1 }

# Check if tag already exists locally
$existingTag = git tag -l $Version
if ($existingTag) {
    Write-Error "Tag '$Version' already exists locally. Delete it first: git tag -d $Version"
    exit 1
}

# Check if tag already exists on remote
$remoteTag = git ls-remote --tags origin "refs/tags/$Version"
if ($remoteTag) {
    Write-Error "Tag '$Version' already exists on remote origin."
    exit 1
}

# Create annotated tag
Write-Host "Creating tag $Version ..." -ForegroundColor Cyan
git tag -a $Version -m "Release $Version"
if ($LASTEXITCODE -ne 0) { exit 1 }

# Push tag to origin
Write-Host "Pushing tag $Version to origin ..." -ForegroundColor Cyan
git push origin $Version
if ($LASTEXITCODE -ne 0) {
    Write-Host "Push failed, removing local tag ..." -ForegroundColor Red
    git tag -d $Version
    exit 1
}

Write-Host ""
Write-Host "Release $Version triggered!" -ForegroundColor Green
Write-Host "Monitor the build at: https://github.com/giovannimazza/rsvpnano/actions" -ForegroundColor Cyan
