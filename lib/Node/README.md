# Node

Purpose: provide immutable node identity and mutable lifecycle status.

Public API: `NodeManager`.

Roadmap: extend configuration, not singleton state, for future identity sources.

Decision: it is an injected object, eliminating hidden process-wide state and making tests independent.
