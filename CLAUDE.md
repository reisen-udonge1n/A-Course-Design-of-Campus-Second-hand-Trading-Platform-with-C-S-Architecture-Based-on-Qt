# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Build both the main app and chat server
cd build && cmake .. && cmake --build .

# Output goes to bin/
# bin/secondhand_trading.exe  — main GUI app
# bin/chat_server.exe         — standalone TCP chat + HTTP image server

# Clean build
cmake --build . --target clean

# Debug vs Release (separate build dirs)
cd build/Desktop_Qt_6_10_3_MinGW_64_bit-Debug && cmake ../.. -DCMAKE_BUILD_TYPE=Debug
```

Requires Qt6 (Widgets/Sql/Network), MariaDB/MySQL, MinGW (tested with 6.10.2 and 6.10.3). Falls back to Qt5 5.15+.

## Project Overview

Campus second-hand trading platform (校园二手交易平台) — Qt desktop app with a separate TCP chat server.

**Two executables:**
- `secondhand_trading.exe` — Qt Widgets GUI (user-facing)
- `chat_server.exe` — headless dual-protocol server: TCP for real-time messaging + HTTP file server (port 9999) for image upload/serve

## Architecture

### Pattern: Singleton services initialized at startup
- `DbManager` — MariaDB/MySQL connection via `QMARIADB`, parameterized raw SQL
- `ChatClient` — TCP client for real-time chat, JSON protocol with 4-byte length prefix
- `VerificationCodeManager` — in-memory email codes, 5-minute expiry
- `SmtpSender` — SSL SMTP via 126.com (AUTH LOGIN, port 465)
- `Filter` — singleton sensitive word filter for content moderation (publish validation)

### App flow: LoginWindow → MainWindow
- `LoginWindow` (logwindow.cpp) — login form from `logwindow.ui`, SHA-256 passwords, 5-attempt lockout
- `MainWindow` (mainwindow.cpp) — `QStackedWidget` with 12 pages + 5-button bottom nav (Home, Products, Publish, Search, Profile)

### 12 pages in QStackedWidget (mainwindow.cpp)
| Index | Class | Purpose |
|-------|-------|---------|
| 0 | HomePage | Welcome header, stats bar (published/purchased/following), category filter tags, dynamic hot-recommendation grid from DB |
| 1 | ProductsPage | Dynamic 2-column product card grid from DB |
| 2 | PublishPage | Product listing form + user's own products list with edit/delete |
| 3 | SearchPage | LIKE query on product titles |
| 4 | ProfilePage | Avatar, stats, menu (edit profile, messages, published, orders) |
| 5-10 | OrderListPage | 6 instances: Published, Purchased, PendingShip, PendingReceipt, PendingConfirm, Received |
| 11 | ChatListPage | Conversation list + inline chat view |

### Dialogs
- `ProductDetailDialog` — product view displaying category, price, description, seller info, address. Buy button opens negotiable-price QInputDialog, auto-creates order + sends offer in chat. Chat button opens ChatDialog.
- `EditProfileDialog` — edit gender, addresses, avatar
- `ChatDialog` — modal chat with ChatBubble widgets
- `ChatBubble` — self=blue right, other=white left
- `FollowersDialog` — two-tab dialog (Following / Discover Users) with search, follow/unfollow
- `NotificationPopup` — floating clickable notification (5s auto-hide) when messages arrive with no open chat

### Content moderation
- `Filter` singleton checks product titles and descriptions against a hardcoded Chinese sensitive-word list. Used in `PublishPage` before insert.

### HomePage lazy-loading pagination
- `HomePage` loads hot-recommendation products in pages of 8 (`PAGE_SIZE`), tracked via `m_offset`/`m_hasMore`. `loadMoreProducts()` is called on scroll-bottom via `eventFilter` on the scroll area viewport. Prevents loading the entire product table at once.

### Notification system (two tiers)
- **In-MainWindow bar**: `MainWindow` has a `QFrame` notification bar (灵动岛通知栏) at the top, shown on incoming chat messages via `onMessageReceived` / `showNotification`, hidden after timeout via `m_notificationTimer`.
- **Popup dialog**: `NotificationPopup` is a floating `QWidget` (5s auto-hide via `m_hideTimer`) that emits `clicked(userId)` — created as a fallback when no chat is open.

### Order lifecycle
0=pending confirm → 2=pending ship → 5=shipped → 3=completed, 4=cancelled. Price negotiation via `offer_price` column on `orders`.

### Image storage
Images stored as comma-separated Base64 data URIs in `products.images` LONGTEXT column. Loaded via `loadPixmap()` in `util.h`.

### Chat protocol
Two persistence paths: messages saved to DB (`messages` table) AND sent via TCP to the chat server. Server routes JSON messages between connected clients and broadcasts user list on connect/disconnect. 30s ping/pong heartbeat, 5s reconnect timer. Duplicate login kicks old session.

## Database (QMARIADB, 6 tables)

- `users` — id, username (unique), email (unique), password_hash (SHA-256)
- `products` — id, title, price, description, seller_id, status (0=active, 1=sold), category, images (LONGTEXT)
- `orders` — id, product_id, buyer_id, seller_id, status, offer_price (nullable DECIMAL)
- `messages` — id, sender_id, receiver_id, product_id, content, is_read
- `user_profiles` — user_id (PK,FK), gender, address, shipping_address, receiving_address, avatar
- `followers` — id, follower_id, followed_id, created_at, UNIQUE(follower_id, followed_id)

Schema evolution via `CREATE TABLE IF NOT EXISTS` + `ALTER TABLE ADD COLUMN` IF NOT EXISTS checks in `DbManager::createTables()`. The DB is auto-created if missing: `DbManager::ensureDatabaseExists()` connects without a database first, runs `CREATE DATABASE IF NOT EXISTS`, then reconnects targeting the new DB.

## Config & Security

All configured in `config.h` (checked into the repo): DB host/user/password, chat server host/port, SMTP credentials.

> **⚠️ config.h contains plaintext production credentials** — it is tracked in git. If forking or making public, move secrets to environment variables or a separate `.env` file excluded by `.gitignore`. `.claude/settings.local.json` is already in `.gitignore` and can be used for local-only overrides.

## Key conventions

- Screen width: 400-420px, height: 600-750px (mobile-form-factor desktop)
- Fusion style + inline Qt stylesheets, card-based design
- Back navigation: each sub-page emits `goBack()` signal
- Click handling on dynamically created widgets: `eventFilter()` override on the parent, checking `QPushButton`/`QLabel` object names and cursor positioning (not `QSignalMapper` or per-item signals)
- UI designer files (`logwindow.ui`) are used only for the login window; all other pages are hand-coded C++
- No ORM, no threading, no unit tests, no CI
