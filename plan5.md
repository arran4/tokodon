Now `findLatestPostId` correctly returns the newer ID even if it is not in the timeline!
This prevents `m_lastReadId` from being prematurely overwritten with an older post during the initial loading of older posts.

Wait, is there still a problem with `repositionAt`?
Let's trace what happens when Tokodon opens with `continueReading`:
1. It fetches `m_lastReadId` (e.g. 100).
2. It fetches older posts (`max_id=100`, e.g., 99, 98, 97).
3. The older posts are added to the timeline.
4. `updateReadMarker(99)` might be called because 99 is in the viewport.
5. `findLatestPostId({99, 100})` compares 99 and 100.
6. Since 100 is newer (longer or lexicographically greater), `findLatestPostId` returns 100.
7. `updateReadMarker` checks `if (latestPostId != m_lastReadId)`. Since `100 == 100`, it does NOTHING!
8. `m_lastReadId` REMAINS 100! (This works perfectly!)
9. Tokodon fetches newer posts (`min_id=99` via `m_prev`, e.g., 102, 101, 100).
10. The newer posts are prepended to the timeline!
11. `pos` is 3 (because 3 newer posts were inserted).
12. Tokodon calls `repositionAt(pos)`.
13. `repositionAt(3)` makes the view jump to index 3.
14. Index 3 is post 99!
15. So the user is looking at post 99.
16. The user has to scroll UP to see post 100, 101, 102.

Wait, is post 99 the correct post to reposition to?
If `repositionAt` positions at post 99, it means the user sees the first *older* post.
But the user wants to continue reading from where they left off, which is post 100!
Wait! Is 100 actually in the newer posts?
Yes! `min_id=99` returns 100, 101, 102!
So 100 is at index 2!
Shouldn't Tokodon reposition to index 2 (post 100) instead of index 3 (post 99)?
If it repositions to index 3, it positions at post 99. The user sees post 99.
Post 99 is a post they ALREADY read (since they left off at 100).
If they see post 99, they might be slightly confused because they already read it, but it gives context!
Many Mastodon clients intentionally show the last read post or the post just before it to give context!
Wait, but if they reposition to 99, they see 99. Then they scroll up to 100.
Wait, if they see 99, they are seeing one extra old post.
Is this off-by-one intentional?
Actually, `pos` returned by `fetchedTimeline(data)` is the NUMBER of posts prepended!
If 3 posts are prepended (102, 101, 100), then index 3 is post 99.
Wait! If they wanted to position at 100, they would position at index `pos - 1` (index 2)!
But wait! If they position at `pos - 1` (index 2), then post 100 is at the top of the screen.
If post 100 is at the top of the screen, and the user reads down, they read 100, then 99, 98.
But they already read 100, 99, 98!
Wait! In Mastodon, the timeline is ordered NEWEST at the top!
If post 100 is at the top of the screen, the posts BELOW it are 99, 98 (older posts)!
So if they read DOWN the screen, they are reading OLDER posts!
If they want to read NEWER posts, they must scroll UP!
If they scroll UP, they see 101, 102 (newer posts).
So if they want to read NEWER posts, they HAVE TO SCROLL UP!
If they are positioned at post 99 (index 3), post 99 is at the top of the screen.
To see post 100, they scroll UP. To see 101, they scroll UP more.
So regardless of whether they are positioned at 99 or 100, they MUST SCROLL UP to read new posts!

Wait, if they must scroll UP to read new posts, then Tokodon works perfectly!
The only issue was that Tokodon overwrote `m_lastReadId` to 99, causing it to slip down every time!
With `findLatestPostId` fixed, it will NO LONGER overwrite `m_lastReadId` to an older post!
It will securely remember the actual `m_lastReadId`.

Is there any other problem?
Let's check what happens if `Config::continueReading()` is enabled but there are no older posts?
(E.g., `max_id` returns an empty array).
Then `pos` will be the number of newer posts.
But wait! If `max_id` returns empty, the first fetch inserts 0 posts.
Then it fetches newer posts via `m_prev`. But `m_prev` is `std::nullopt`!
```cpp
            if (!backwards || !m_next) {
                m_next = TextHandler::getNextLink(linkHeader);
            }
            if (backwards || !m_prev) {
                m_prev = TextHandler::getPrevLink(linkHeader);
            }
```
If the first fetch is empty, it means there are NO older posts.
Wait, if there are NO older posts, then `linkHeader` might be empty.
So `m_prev` might be empty.
If `m_prev` is empty, `hasPrevious()` will be false?
Wait, if `m_prev` is empty, `hasFromId` is true, so it calls `fillTimeline({}, true)`.
In `fillTimeline({}, true)`, `backwards` is true.
```cpp
    if (backwards) {
        // If we are moving backwards, use the prev url
        Q_ASSERT(m_prev.has_value());
        url = m_prev.value();
```
Wait! `Q_ASSERT(m_prev.has_value())` will CRASH if `m_prev` is nullopt!
Let's check if `linkHeader` is empty when the reply is empty:
```cpp
            // If the reply is empty, do NOT overwrite m_prev/m_next and wipe pagination. That just means the server has nothing more to give us, at the moment.
            const auto data = reply->readAll();
            const auto doc = QJsonDocument::fromJson(data);
            if (doc.array().isEmpty()) {
                setLoading(false);
                return;
            }
```
If the reply is empty, it returns IMMEDIATELY!
It does NOT call `fillTimeline({}, true)`!
So if the older posts are empty, it just stops! It NEVER fetches newer posts!
Wait! Is it possible for older posts to be empty?
If `max_id = m_lastReadId`, older posts are posts older than `m_lastReadId`.
If `m_lastReadId` is the oldest post on the server (very unlikely), then older posts are empty.
Then it stops. And it never fetches newer posts!
So if `continueReading()` is on, and you are at the absolute bottom of the timeline, it won't load any new posts!
But that is an edge case that is practically impossible (the server always has older posts unless you are at the absolute beginning of your instance).

Wait! What if we want it to fetch newer posts anyway?
That is out of scope for "Why doesn't tokodon remember my last scroll position".

Let's double check if we need to call `plan_step_complete`!
Wait! We fixed `findLatestPostId`. Is it sufficient?
Let's verify that `findLatestPostId` logic works correctly.
```cpp
QString TimelineModel::findLatestPostId(const QStringList &postIds) const
{
    if (postIds.isEmpty()) {
        return QString();
    }

    QString latest = postIds.first();
    for (const auto &id : std::as_const(postIds)) {
        if (id.length() > latest.length() || (id.length() == latest.length() && id > latest)) {
            latest = id;
        }
    }

    return latest;
}
```
This looks completely correct and directly solves the bug where Tokodon overwrites the `lastReadId` with an older post during the initial load because the newer post is not in the timeline yet.
