#!/usr/bin/env powershell

param (
    [Parameter(Mandatory=$true)][string]$srcfile,
    [string]$dstfile = $srcfile
)

$content = Get-Content -Raw $srcfile
$fileext = [System.IO.Path]::GetExtension($srcfile)
$lichead = Get-Content -Raw "$PSScriptRoot\header$fileext"
$content = $content -replace "(?sm)/\*((?!\*/).)*This file is part of((?!\*/).)*\*/", $lichead
Set-Content -Path $dstfile -NoNewline -Value $content
