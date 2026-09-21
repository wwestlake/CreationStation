#!/usr/bin/env python3
"""Writes the help topics about the assistant's saved conversations (help/topics/en-US/djehuti.station.conversations-*.json).

What is recorded, where, how the chain of blocks protects it, what it does and does not guarantee, and how to open, archive,
export and delete a conversation. Written from what the code does (shared/AssistantStore, Agent/StationConversations.cpp).
Run from the Station folder:  python help/tools/make_conversation_topics.py
"""

import json
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "topics" / "en-US"


def topic(topic_id, kind, title, summary, keywords, questions, blocks, related=()):
    return {
        "schemaVersion": "1.0", "id": topic_id, "locale": "en-US", "type": kind, "title": title, "summary": summary,
        "product": "Djehuti Station", "status": "draft", "audiences": ["user", "support", "virtual-engineer"],
        "owners": ["Djehuti Station"], "appliesTo": {"since": "0.9.1-beta6", "platforms": ["windows"]}, "contexts": [],
        "keywords": list(keywords), "alternateQueries": list(questions),
        "relations": [{"type": "next", "topicId": r} for r in related],
        "blocks": [{"id": f"b{i}", "kind": k, "text": t} if k in ("paragraph", "note", "warning") else dict(t, id=f"b{i}", kind=k)
                   for i, (k, t) in enumerate(blocks, start=1)],
        "contentRevision": 1, "reviewedAt": "2026-09-21",
        "reviewedBy": "Claude (from the code as built, not yet reviewed by a person)",
    }


def para(text): return ("paragraph", text)
def note(text): return ("note", text)
def warn(text): return ("warning", text)
def define(term, meaning): return ("definition", {"term": term, "definition": meaning})


TOPICS = [
    topic(
        "djehuti.station.conversations-saved-securely",
        "explanation",
        "Your conversations with the assistant are saved as a tamper-evident chain",
        "How the assistant's conversations are recorded: what is saved and where, how each message is chained to the one before it, "
        "what that protects against, and what it does not.",
        ["conversation", "conversations", "saved", "history", "record", "recorded", "blockchain", "block chain", "chain", "ledger",
         "hash", "sha-256", "tamper", "tamper-evident", "altered", "integrity", "verify", "secure", "immutable", "signed", "signing",
         "audit", "log", "chat history"],
        ["Are my conversations with the assistant saved?", "Where are my conversations stored?", "Is the conversation history secure?",
         "Can a saved conversation be edited?", "What does ALTERED mean?", "Is it a blockchain?", "How do I know a conversation was not changed?",
         "What is saved in a conversation?", "Can the assistant change its own saved conversations?"],
        [
            para("Every conversation you have with the assistant is saved automatically, as you talk, inside the project you have open. Your "
                 "message is saved the moment you send it, before the assistant starts work, and the answer is saved when it arrives. Nothing "
                 "is lost if Station closes: the conversation is already in the project."),
            define("Where it is kept", "In the project's own storage, in the assistant's area for Station: Assistants/Station/conversations, one file per "
                                       "conversation. Because it lives in the project, it travels with the project and is private to whoever owns it. If no "
                                       "project is open, there is nowhere to save, and Station says so instead of saving silently."),
            define("What is saved", "Only what you can see in the panel: your messages and the assistant's answers (including the scripts an answer "
                                    "shows and the note of which help topics it used). Not saved: the assistant's hidden instructions, your account keys, "
                                    "the help excerpts and project context added behind the scenes, or the assistant's inner tool steps."),
            define("The chain", "Each message is a block. Every block carries a fingerprint (a SHA-256 hash) worked out from its own content and from the "
                                "fingerprint of the block before it. This is the same idea a blockchain uses to link blocks, on your own machine only: "
                                "there is no network, no mining and no coin. Because each block depends on the one before, changing, inserting, removing or "
                                "reordering any message breaks every fingerprint after it, and the break is found when the conversation is loaded."),
            define("What that protects against", "It makes changes detectable. When a conversation is opened, Station checks the whole chain. If anything "
                                                 "does not match, the conversation is marked [ALTERED] in the Chats list, it can never be opened or fed back "
                                                 "to the assistant, and it can only be deleted or left as evidence. One damaged file never stops you starting a fresh conversation."),
            define("Where the protection stops", "It is tamper-evident, not tamper-proof. Someone with the ability to rewrite the whole file could recalculate "
                                                 "every fingerprint and produce a new chain that looks valid. To close that gap the design adds signing later "
                                                 "(for example a key kept privately by each app, with the public half registered on the website). That is not built "
                                                 "yet, so today the chain protects against accidental damage and casual edits, not a determined forger."),
            note("The assistant has no tool for reading or changing its saved conversations. It cannot rewrite its own record."),
        ],
        related=("djehuti.station.conversations-managing", "djehuti.station.virtual-engineer"),
    ),
    topic(
        "djehuti.station.conversations-managing",
        "how-to",
        "Open, continue, archive, export and delete conversations (Chats)",
        "How to use the Chats window: see your saved conversations, continue one, start a new one, put one away, restore it, export it "
        "to a file, or delete it.",
        ["chats", "conversation list", "list of conversations", "open conversation", "continue conversation", "new conversation", "archive",
         "restore", "export", "delete conversation", "markdown", "json", "history", "past conversations", "saved chats"],
        ["How do I see my past conversations?", "How do I continue an old conversation?", "How do I start a new conversation?",
         "How do I delete a conversation?", "How do I export a conversation?", "How do I archive a conversation?",
         "Where is the Chats button?", "How do I get an archived conversation back?"],
        [
            define("Chats", "The Chats button is in the row under the assistant panel's title, next to the Engineer/Producer box. It opens a window that lists "
                            "this project's conversations, newest first: the title (taken from your first message), the date and time it was last used, and how many "
                            "messages it holds."),
            define("Open and continue", "Select a conversation and press Open (or double-click it). Its messages appear in the panel and the assistant is given "
                                        "the conversation as its memory, so you can carry on where you stopped. You cannot open one while the assistant is still "
                                        "working: let it finish or press Stop."),
            define("New", "The New button starts a fresh conversation. The one you were in stays saved in Chats. Until you send your first message, nothing new is saved."),
            define("Archive and restore", "Archive puts a conversation away: it leaves the main list but is kept exactly as it was. Turn on Show archived to see "
                                          "archived conversations, and press Restore to bring one back. Only a conversation that passes its integrity check can be archived."),
            define("Export", "Export saves a conversation to a file you choose. Name it with .md to get a readable document (title, dates, then each turn), or "
                             "with .json to get the verifiable record exactly as stored, which anyone can check against the chain. A conversation that fails its integrity check cannot be exported."),
            define("Delete", "Delete removes a conversation for good, after a confirmation. It cannot be undone. A conversation marked [ALTERED] can be deleted "
                             "even though it cannot be opened. Deleting the conversation on screen means your next message starts a new one."),
            note("Conversations belong to the project. Open a different project and you see that project's conversations."),
        ],
        related=("djehuti.station.conversations-saved-securely",),
    ),
]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for t in TOPICS:
        with open(OUT / f"{t['id']}.json", "w", encoding="utf-8", newline="\n") as out:
            out.write(json.dumps(t, indent=2, ensure_ascii=False) + "\n")
        print("wrote", t["id"])


if __name__ == "__main__":
    main()
