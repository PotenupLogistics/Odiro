---
id: agents-policy-rag-data
owner: Agents
paths:
  - Agents/app/services/*rag*
  - Agents/app/services/*policy*
  - Agents/data/**
  - Agents/docs/policy/**
  - Agents/docs/rag/**
  - Agents/docs/database/**
  - Agents/scripts/*policy*
  - Agents/scripts/*rag*
entry:
  - Agents/app/services/policy_rag_retriever.py
  - Agents/app/services/policy_recommendation_orchestrator.py
  - Agents/app/services/policy_source_analyzer.py
  - Agents/data/rag
  - Agents/data/sources
  - Agents/scripts/generate_policy_cards.py
  - Agents/scripts/generate_rag_chunks.py
keep:
  - Agents/data is source/processed/review data; only release runtime read-only assets move to Agents/static.
  - Raw PDFs and manual review packs are not release assets by default.
verify:
  - file-based RAG readiness
  - policy card/chunk validation
  - retrieval/recommendation focused tests
related:
  - agents-generation-runtime
  - client-delivery-bot-policy
---
