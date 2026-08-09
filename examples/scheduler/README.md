# Scheduler Tuning Reference Adapter

`scheduler_tuning.c` searches quantum, logical-lane, batch, and backoff values
against six explicit immutable jobs. Lane/batch and quantum/backoff relations
are hard constraints; oversized batches receive a visible soft penalty.

The evaluator declares `EVO_EVALUATION_CALLBACK_THREAD_SAFE` because it only
reads the immutable fixture and its input genome. Three bounded workers are
requested with the exact public scratch-size query. Every generation emits all
12 candidate assignments in population order, including stable logical worker
identity, dispatch wave, final disposition, commit presence, and commit order.
Native thread IDs, callback completion timing, and queue internals are neither
recorded nor authoritative.

The fixture models one queue. It does not install or alter an operating-system
scheduler and makes no latency, throughput, fairness, or safety claim outside
the declared integer model.
