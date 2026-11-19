# How Claude reports a test (gates 3, 4 and 5)

Set up 2026-11-19 after a week of claims Mark could not rely on ("the shader fix worked" about a build that did not
have it, "your next build includes this", first boots that failed for causes a check would have found).

## Gate 3: read the log with the script first

`python3 tools/read_log.py --logs out/logs --expect <latest pushed commit>` on Mark's PC (device shell) before any
statement about a test. If it prints WRONG BUILD, the reply says so in its first line and nothing about the test.

## Gate 4: every report has this shape

1. **Build:** the commit the log says, and that it matches the latest push (from read_log.py).
2. **What the log shows:** facts only, each with where it comes from (a read_log.py line, a log line, a capture).
3. **What that means:** a cause only when the evidence proves it; otherwise "I think X; this test checks X".
4. **Not checked:** what the run could not tell us.
5. **Next:** the plan, waiting for Mark's go-ahead.

Never: "worked", "fixed", "is in your build", "will show" without the line that proves it. A change is "in the build"
only when read_log.py shows that build's commit, and "working" only when its own log line shows it in force.

## Gate 5: an independent check at the big handoffs

Before Mark gets: a port's first boot test, a release, or the next test after two failed tests in a row, a separate
agent that has not seen the work checks the claims. Claude gives it the draft message, `out/preflight.txt`, the repo
and the logs, with this brief:

> You are checking a message to Mark before it is sent. For every factual claim in it (what a build contains, what a
> log shows, what a fix does, what to expect), find the evidence in the repo, the preflight report or the logs, and
> mark it SUPPORTED (quote the evidence), UNSUPPORTED (nothing shows it) or WRONG (the evidence says otherwise). Check
> that the build commit in the logs matches the latest push. List anything the message promises that nothing backs.
> Do not fix anything; report only.

Every UNSUPPORTED or WRONG claim is fixed or removed before the message goes out.

## Gate 2 is in front of all of this

`python3 tools/preflight.py` (plus `--codegen --full` when hooks, overrides or code generation changed) must report
PASS before Mark gets build commands; the reply quotes its RESULT line.
