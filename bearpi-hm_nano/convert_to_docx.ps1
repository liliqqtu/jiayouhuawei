$word = New-Object -ComObject Word.Application
$word.Visible = $false
$doc = $word.Documents.Add()
$selection = $word.Selection

$content = [System.IO.File]::ReadAllLines('D:\Dream\bearpi-hm_nano_1\design_proposal.md', [System.Text.Encoding]::UTF8)

foreach ($line in $content) {
    $trimmed = $line.Trim()
    if ($trimmed.StartsWith('# ')) {
        $selection.Font.Name = '黑体'
        $selection.Font.Size = 18
        $selection.Font.Bold = 1
        $selection.ParagraphFormat.Alignment = 1 # Center
        $selection.TypeText($trimmed.Substring(2) + "`n")
        $selection.ParagraphFormat.Alignment = 0 # Left
    } elseif ($trimmed.StartsWith('## ')) {
        $selection.Font.Name = '黑体'
        $selection.Font.Size = 14
        $selection.Font.Bold = 1
        $selection.ParagraphFormat.SpaceBefore = 8
        $selection.ParagraphFormat.SpaceAfter = 4
        $selection.TypeText($trimmed.Substring(3) + "`n")
    } elseif ($trimmed.StartsWith('### ')) {
        $selection.Font.Name = '黑体'
        $selection.Font.Size = 13
        $selection.Font.Bold = 1
        $selection.ParagraphFormat.SpaceBefore = 6
        $selection.ParagraphFormat.SpaceAfter = 2
        $selection.TypeText($trimmed.Substring(4) + "`n")
    } elseif ($trimmed.StartsWith('#### ')) {
        $selection.Font.Name = '楷体'
        $selection.Font.Size = 12
        $selection.Font.Bold = 1
        $selection.TypeText($trimmed.Substring(5) + "`n")
    } elseif ($trimmed.Length -eq 0) {
        # 空行
    } else {
        $selection.Font.Name = '宋体'
        $selection.Font.Size = 12 # 小四号
        $selection.Font.Bold = 0
        $selection.ParagraphFormat.LineSpacingRule = 1 # 1.5倍行距
        $selection.ParagraphFormat.SpaceBefore = 0
        $selection.ParagraphFormat.SpaceAfter = 0
        $cleanText = $trimmed -replace '\*\*', '' -replace '`', ''
        $selection.TypeText($cleanText + "`n")
    }
}

$savePath = 'D:\Dream\bearpi-hm_nano_1\独居老人居家安全监测与应急辅助系统_设计方案.docx'
if (Test-Path $savePath) {
    Remove-Item $savePath -Force
}
$doc.SaveAs([ref]$savePath, [ref]16)
$doc.Close()
$word.Quit()
Write-Output 'DOCX_EXPORT_OK'
