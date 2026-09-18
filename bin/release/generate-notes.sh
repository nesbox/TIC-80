#!/usr/bin/env bash
# generate-notes.sh — print the release body: our summary, then GitHub's list.
#
#   DEEPSEEK_API_KEY=... generate-notes.sh <tag>
#
# Two halves, each best-effort, so one failing never costs the other:
#
#   1. a readable summary written by DeepSeek from the merged pull request
#      titles of the range — what the release means, grouped for players;
#   2. the list GitHub generates for the same range — every pull request
#      with its author and link, the first-time contributors, the compare
#      link. This is the half that credits people, and prose must never be
#      the only thing on the page: a summary that replaces the list loses
#      the contributors and the per-PR links the release is read for.
#
# Both halves describe one range, and it is the last *published* release.
# The nearest git tag is not that range in this repo: releases are cut from
# `stable`, whose tags are not ancestors of `main` (the nearest ancestor tag
# is v0.90.1706 of 2021), so `git describe` would describe three and a half
# extra years — the v1.2.0 draft written this way says "since v0.90.1706"
# over 1448 commits and 321 pull requests, where GitHub counts the ~250 pull
# requests merged since v1.1.2837. GitHub's own generate-notes defaults to
# the same last published release; naming it explicitly keeps the two halves
# describing one set of changes.
#
# Prints the body to stdout and exits non-zero only when both halves are
# empty, so release.yml can leave the draft's body alone in that case.

set -uo pipefail

TAG="${1:?usage: generate-notes.sh <tag>}"
KEY="${DEEPSEEK_API_KEY:?DEEPSEEK_API_KEY is not set}"
REPO="nesbox/TIC-80"

# The range: the last published release. The API's /releases/latest skips
# drafts (the tag being released is one) and prereleases, so this is the
# version users actually have. No published release at all (a first
# release) falls back to the newest tag in history and then to the root
# commit, so the window is always well defined.
PREV_JSON="$(gh api "repos/$REPO/releases/latest" 2>/dev/null || true)"
PREV_TAG="$(jq -r '.tag_name // empty' <<<"$PREV_JSON" 2>/dev/null)"
SINCE="$(jq -r '(.published_at // .created_at // empty) | .[0:10]' <<<"$PREV_JSON" 2>/dev/null)"
if [ -z "$PREV_TAG" ]; then
    PREV_TAG="$(git describe --tags --abbrev=0 "$TAG^" 2>/dev/null || true)"
fi
# PREV_TAG names the range; PREV_REF is what git can be asked about. They
# are the same for a real release and part ways only for a first one, where
# the range is everything up to the root commit.
PREV_REF="$PREV_TAG"
if [ -z "$PREV_TAG" ]; then
    PREV_TAG="(first release)"
    PREV_REF="$(git rev-list --max-parents=0 HEAD)"
fi
if [ -z "$SINCE" ]; then
    SINCE="$(git log -1 --format=%cs "$PREV_REF" 2>/dev/null || echo 2000-01-01)"
fi

# The pull request titles of the range, for the model. The count and the
# titles come from one response: this is the largest query in the script (a
# thousand pull requests), and asking twice let a pull request merged in
# between be counted and not listed, or the other way round.
PR_TITLES="$(gh pr list --repo "$REPO" --state merged --base main --limit 1000 \
    --search "merged:>=$SINCE" --json title 2>/dev/null || true)"

# --- half one: our summary -------------------------------------------------
# An empty or unparseable answer is a failed query, not a release with no
# pull requests: writing a summary from zero titles produces confident prose
# about nothing, which is worse than no summary at all.
SUMMARY=""
if jq -e 'type == "array"' >/dev/null 2>&1 <<<"$PR_TITLES"; then
    NPRS="$(jq 'length' <<<"$PR_TITLES")"
    PRS="$(jq -r '.[].title' <<<"$PR_TITLES")"
    COMMITS="$(git rev-list --count "$PREV_REF"..HEAD 2>/dev/null || echo unknown)"
    CONTRIBS="$(git shortlog -sn "$PREV_REF"..HEAD 2>/dev/null | wc -l | tr -d ' ')"

    PROMPT="Write release notes for TIC-80 $TAG (a fantasy computer for making, playing and sharing tiny games). The previous release was $PREV_TAG.

Here are the titles of the merged pull requests since $PREV_TAG:

$PRS

Write concise, readable Markdown release notes for players — not a raw PR list. Group them into sections: a one-sentence summary, \"What's new\", \"Platform\", \"Bug fixes\", \"Build & infrastructure\". End with a Statistics line: $COMMITS commits, $NPRS pull requests, $CONTRIBS contributors."

    SUMMARY="$(jq -n --arg model "deepseek-v4-flash" --arg content "$PROMPT" \
        '{model: $model, messages: [{role: "user", content: $content}], temperature: 0.3}' \
        | curl -sS -f https://api.deepseek.com/chat/completions \
            -H "Authorization: Bearer $KEY" -H "Content-Type: application/json" \
            -d @- \
        | jq -er '.choices[0].message.content' 2>/dev/null || true)"
fi
if [ -n "$SUMMARY" ]; then
    printf '%s\n\n' "$SUMMARY"
else
    echo "generate-notes: no summary (model, key or the pull request query) — the body keeps the list" >&2
fi

# --- half two: GitHub's list -----------------------------------------------
# previous_tag_name pins the range to the one the summary was written for;
# without a published release, the endpoint's own default is the right one.
gh_args=(-f "tag_name=$TAG")
if [ "$PREV_TAG" != "(first release)" ]; then
    gh_args+=(-f "previous_tag_name=$PREV_TAG")
fi
LIST="$(gh api "repos/$REPO/releases/generate-notes" -X POST "${gh_args[@]}" --jq .body 2>/dev/null || true)"
if [ -n "$LIST" ]; then
    printf '%s\n' "$LIST"
else
    echo "generate-notes: GitHub's list failed — see the error above" >&2
fi

# Nothing at all: let the caller keep the draft's existing body.
[ -n "$SUMMARY$LIST" ]
