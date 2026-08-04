# Evolutionary Search Theory

EVO treats optimization as a population-based search over encoded candidate solutions.

Each run defines:

- a genome representation,
- a phenotype or evaluated configuration,
- a fitness function,
- constraints,
- selection pressure,
- crossover behavior,
- mutation behavior,
- diversity controls,
- and stopping criteria.

The core supports versioned tournament and stable rank-based selection over
natural problem representations, explicit diversity measurement, and
reproducible seeded runs. Adaptive mutation remains a dependency-ordered
roadmap capability.

Premature convergence, genetic drift, epistasis, deceptive fitness landscapes, and loss of useful building blocks are treated as engineering risks that must be measured rather than assumed away.
