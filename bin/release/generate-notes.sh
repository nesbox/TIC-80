#!/usr/bin/env bash
# generate-notes.sh — generate categorized release notes via DeepSeek and
# print them to stdout. release.yml uses it to fill the draft release body.
#
#   DEEPSEEK_API_KEY=... generate-notes.sh <tag>
#
# The "previous" version is the latest *published* release (the new tag is a
# draft, so it isn't published yet). PR titles and commit/contributor stats
# are collected from git and the GitHub API, then summarized by the model.

set -euo pipefail

TAG="${1:?usage: generate-notes.sh <tag>}"
KEY="${DEEPSEEK_API_KEY:?DEEPSEEK_API_KEY is not set}"
REPO="nesbox/TIC-80"

# The previous release is the parent git tag, not the latest *published*
# GitHub release: the tag being released is still a draft at this point, and
# git works even for the very first release. Falls back to the repo's root
# commit so the stats window is always valid.
PREV_REF="$(git describe --tags --abbrev=0 HEAD^ 2>/dev/null || echo "")"
if [ -n "$PREV_REF" ]; then
    PREV_TAG="$PREV_REF"
    SINCE="$(git log -1 --format=%cs "$PREV_REF" 2>/dev/null || echo 2000-01-01)"
else
    PREV_TAG="(first release)"
    ROOT="$(git rev-list --max-parents=0 HEAD)"
    PREV_REF="$ROOT"
    SINCE="$(git log -1 --format=%cs "$ROOT" 2>/dev/null || echo 2000-01-01)"
fi

NPRS="$(gh pr list --repo "$REPO" --state merged --base main --limit 1000 \
    --search "merged:>=$SINCE" --json title --jq 'length')"
PRS="$(gh pr list --repo "$REPO" --state merged --base main --limit 1000 \
    --search "merged:>=$SINCE" --json title --jq '.[].title')"

COMMITS="$(git rev-list --count "$PREV_REF"..HEAD 2>/dev/null || echo unknown)"
CONTRIBS="$(git shortlog -sn "$PREV_REF"..HEAD 2>/dev/null | wc -l | tr -d ' ')"

PROMPT="Write release notes for TIC-80 $TAG (a fantasy computer for making, playing and sharing tiny games). The previous release was $PREV_TAG.

Here are the titles of the merged pull requests since $PREV_TAG:

$PRS

Write concise, readable Markdown release notes for players — not a raw PR list. Group them into sections: a one-sentence summary, \"What's new\", \"Platform\", \"Bug fixes\", \"Build & infrastructure\". End with a Statistics line: $COMMITS commits, $NPRS pull requests, $CONTRIBS contributors."

jq -n --arg model "deepseek-v4-flash" --arg content "$PROMPT" \
    '{model: $model, messages: [{role: "user", content: $content}], temperature: 0.3}' \
    | curl -s https://api.deepseek.com/chat/completions \
        -H "Authorization: Bearer $KEY" -H "Content-Type: application/json" \
        -d @- \
    | jq -r '.choices[0].message.content'
