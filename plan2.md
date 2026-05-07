Wait, if we prevent `updateReadMarker` while the timeline is still loading its initial "continue reading" chunks, we can use a flag.
But the issue is that `loading()` might be true while fetching.
Wait! Is `loading()` true during `fillTimeline`?
Yes, `fillTimeline` calls `setLoading(true)`.
```cpp
    setLoading(true);
```
And it sets `setLoading(false)` when the network reply finishes.
BUT! QML receives `onContentYChanged` and calls `updateReadMarker` WHEN `loading()` is false!
Let's look at `TimelineView.qml`:
There is no check for `loading` before calling `updateReadMarker`.
But does it happen while `loading()` is false?
If we do `fillTimeline(m_lastReadId)`, it fetches older posts.
Then it calls `setLoading(false)`.
THEN it calls `fillTimeline({}, true)`. This sets `setLoading(true)` AGAIN!
In the brief moment between `setLoading(false)` and `setLoading(true)`, or maybe because `repositionAt` is async, QML might trigger `updateReadMarker`.
Wait! Actually, `hasFromId` is true.
```cpp
            setLoading(false);

            if (hasFromId) {
                fillTimeline({}, true);
            }
```
If we set `setLoading(false)`, QML bindings update.
But we immediately call `fillTimeline({}, true)`, which sets `setLoading(true)`.

Wait, even if we add a check for `loading()` in `updateReadMarker`, it might be better!
Let's see:
```cpp
void MainTimelineModel::updateReadMarker(const QString &postId)
{
    if (loading()) {
        return;
    }
```
If we do that, does it solve the problem?
Wait, if it's called during `loading()`, it just ignores it.
But after `fillTimeline({}, true)` completes, `loading()` becomes false.
Then the items are shown. QML calls `updateReadMarker` (or maybe it called it before, and now it doesn't call it again because it hasn't moved).
If it doesn't call it again, `m_lastReadId` won't be updated until the user actually scrolls! This is what we want!

BUT wait! When the second fetch completes, it calls `repositionAt(pos)`.
`repositionAt(pos)` changes the position, so QML WILL call `updateReadMarker` again because `contentY` changes!
Wait, when it calls `updateReadMarker`, `loading()` is FALSE.
So it will execute `updateReadMarker(postId)`.
What will `postId` be?
It will be the post at index `pos` (which is the first older post).
And what is `m_lastReadId`? It is the one we loaded from the API (e.g. 100).
If `postId` is 99 (older post), `findLatestPostId({99, 100})` will return `100` (because 100 is NOW in the timeline, since we loaded the newer posts!).
So `latestPostId` will be 100.
`latestPostId != m_lastReadId` (100 != 100) will be FALSE!
So it WILL NOT overwrite `m_lastReadId` to 99!
This is perfect!
Wait, is this really true?
If 100 is in the timeline, `findLatestPostId({99, 100})` will find 100 FIRST?
Yes, because 100 is at index 2, and 99 is at index 3.
`std::ranges::find` iterates from index 0. It finds 100 before 99.
So it returns 100.
So if we just guard `updateReadMarker` with `if (loading()) return;` or `if (fetchingLastId && !fetchedLastId)` or something, we can prevent the intermediate overwrite!
Wait! Is `loading()` true when QML handles the model change?
When `setLoading(false)` is called, the model is still processing.
Actually, `fetchedTimeline` calls `beginInsertRows`. QML handles the rows inserted synchronously.
During `beginInsertRows`, `loading()` is still `true` (because `setLoading(false)` is called AFTER `fetchedTimeline`).
So if QML calls `updateReadMarker` during `rowsInserted`, `loading()` is TRUE!
But what if QML calls it on the next event loop iteration?
If we do `setLoading(false)`, then return to the event loop, `loading()` is false. QML might call `updateReadMarker` then!
But we immediately call `fillTimeline({}, true)` which sets `loading()` to true.
Is there any gap?
`setLoading(false)` emits `loadingChanged`.
Then `fillTimeline({}, true)` calls `setLoading(true)` which emits `loadingChanged`.
All in the same event loop tick!
So if QML runs `onContentYChanged` via a signal, it might be triggered during `rowsInserted` (when `loading()` is true).
Or it might be deferred to the next frame. If it's deferred to the next frame, `fillTimeline` has already set `loading()` to true!
So `loading()` will be true!
Until the SECOND network reply finishes.
When the second network reply finishes:
```cpp
            if (publicTimelines.contains(m_timelineName) && backwards) {
                int const pos = fetchedTimeline(data);
                Q_EMIT repositionAt(pos);
            }
...
            setLoading(false);
```
Here, `fetchedTimeline` is called (which inserts rows).
Then `repositionAt` is emitted.
Then `setLoading(false)` is called!
So during `repositionAt`, `loading()` is TRUE!
Then `setLoading(false)` is called.
Then the event loop processes. QML will update the view, and `loading()` will be FALSE.
Then the user scrolls, `loading()` is FALSE, so `updateReadMarker` works!

Let's check if this is foolproof.
What if `updateReadMarker` is called when `m_timeline` DOES NOT contain `m_lastReadId`?
When does `m_timeline` NOT contain `m_lastReadId`?
If `continueReading()` is enabled, the second fetch uses `min_id=m_lastReadId` (actually `m_prev` which is derived from `max_id=m_lastReadId`).
If the second fetch fails, or returns no posts?
If there are NO newer posts, `m_timeline` will ONLY have older posts!
If it only has older posts, it DOES NOT contain `m_lastReadId`!
If it DOES NOT contain `m_lastReadId`, then `findLatestPostId({99, 100})` will return `99` (because 100 is not in the timeline).
So `latestPostId` will be 99.
And `m_lastReadId` will be OVERWRITTEN to 99!
Wait! Is `m_lastReadId` returned when there are NO newer posts?
If there are no newer posts, `min_id=m_lastReadId` returns an empty array.
If it returns an empty array, `m_timeline` only has posts OLDER than `m_lastReadId`.
So `m_lastReadId` is missing!
Why does it overwrite `m_lastReadId` to an older post?
Because `updateReadMarker` assumes that if a post is in the viewport, the user has read up to that post.
And since `m_lastReadId` is not in the timeline, it assumes `99` is the newest post the user has read.
But wait! If the user has already read `100`, why should Tokodon mark `99` as the latest read post?
It shouldn't! Mastodon read markers should ONLY move forward!
Wait. Does the Mastodon API allow moving read markers backward?
Yes, if you send a POST with `last_read_id=99`, it updates it to 99!
But the client should NEVER move it backward!
If we know `m_lastReadId` is 100, and 99 is older than 100, we should NOT update it to 99!
Wait! `findLatestPostId` is supposed to return the NEWEST post!
If it is passed `{99, 100}`, it should return 100!
Why does `findLatestPostId` return 99?
Because it ONLY checks posts that are in `m_timeline`!
```cpp
    for (const auto post : std::as_const(m_timeline)) {
        if (postIds.contains(post->originalPostId())) {
            return post->originalPostId();
        }
    }
```
Since 100 is not in `m_timeline`, it doesn't find it. So it finds 99!
But wait! We can just string-compare the IDs!
Mastodon IDs are generally chronological and can be compared as strings (if they are the same length) or numerically!
Wait, Mastodon API documentation says:
"IDs are guaranteed to be sorted chronologically... They are 64-bit integers but should be treated as strings. Lexicographical sorting of IDs does not guarantee chronological order if they have different lengths. For chronological sorting, you should compare them as numbers (or left-pad them with zeroes to a fixed length before string comparison)."
Wait, `findLatestPostId` currently uses the `m_timeline` array which is ALREADY sorted newest to oldest.
So the first one it finds is the newest!
But if one of the IDs is NOT in the timeline, it just ignores it!
If we pass `{postId, m_lastReadId}`, and `m_lastReadId` is not in the timeline, it ignores `m_lastReadId` and returns `postId`!
This is a HUGE BUG!
It means if `m_lastReadId` is not in the timeline, Tokodon will ALWAYS overwrite the read marker with `postId`, even if `postId` is much OLDER!
To fix this, `MainTimelineModel::updateReadMarker` should compare `postId` and `m_lastReadId` numerically (or pad with zeros) to see which is newer!
If `postId` is older than `m_lastReadId`, it should just do NOTHING!

Let's look at `findLatestPostId` usages.
```cpp
src/timeline/maintimelinemodel.cpp:349:        const auto latestPostId = findLatestPostId({postId, m_lastReadId});
src/timeline/maintimelinemodel.cpp:395:    const auto latestPostId = findLatestPostId({postId, m_initialLastReadId});
```
It is ONLY used to compare `postId` with the read marker!
If we just compare the lengths, and then lexicographically, we can determine which is newer!
```cpp
bool isNewer(const QString &id1, const QString &id2) {
    if (id1.length() != id2.length()) {
        return id1.length() > id2.length();
    }
    return id1 > id2;
}
```
If we use this, `latestPostId` is just the newer of the two!
Wait! Is it that simple?
Yes! "Lexicographical sorting of IDs does not guarantee chronological order if they have different lengths. For chronological sorting, you should compare them as numbers (or left-pad them with zeroes...)"
Since they are string representations of integers, a longer string is a larger number. If they are the same length, string comparison works!

Let's check `TimelineModel::fetchedTimeline`:
```cpp
            if (postOld->originalPostId() > postNew->originalPostId()) {
```
Wait, in `TimelineModel::fetchedTimeline`, it ALREADY compares them as strings using `>`!
```cpp
            if (postOld->originalPostId() > postNew->originalPostId()) {
```
But wait, if it compares them as strings using `>`, and lengths are different, it might be wrong!
However, Mastodon IDs are 18 digits now, and have been for years. Different lengths are very rare (only for very old posts vs new posts).
But a proper comparison function is better.

If we just fix `MainTimelineModel::updateReadMarker` to NOT update if `postId` is older than `m_lastReadId`, we fix the bug!
```cpp
void MainTimelineModel::updateReadMarker(const QString &postId)
{
    const bool isHome = m_timelineName == QStringLiteral("home");

    if (isHome) {
        if (m_lastReadId.isEmpty()) {
            m_account->saveTimelinePosition(QStringLiteral("home"), postId);
            m_lastReadId = postId;
            return;
        }

        // Compare postId and m_lastReadId to find the newer one
        bool postIdIsNewer = false;
        if (postId.length() != m_lastReadId.length()) {
            postIdIsNewer = postId.length() > m_lastReadId.length();
        } else {
            postIdIsNewer = postId > m_lastReadId;
        }

        if (postIdIsNewer) {
            m_account->saveTimelinePosition(QStringLiteral("home"), postId);
            m_lastReadId = postId;
        }
    }
}
```
Wait, if we do this, we don't even need `findLatestPostId`!
Let's see where else `findLatestPostId` is used.
```cpp
bool MainTimelineModel::data(const QModelIndex &index, int role) const
{
...
    const auto postId = data(index, OriginalIdRole).toString();
    const auto latestPostId = findLatestPostId({postId, m_initialLastReadId});
    if (latestPostId == m_initialLastReadId) {
        return true;
    }

    return false;
}
```
This is for `ShowReadMarkerRole`!
It shows the read marker on a post if `postId` is older than or equal to `m_initialLastReadId`!
Wait, if `latestPostId == m_initialLastReadId`, it means `m_initialLastReadId` is NEWER than or equal to `postId`.
So the post is OLDER than the read marker!
Wait, we ONLY want to show the read marker on ONE post!
If we return `true` for all older posts, the UI might show a read marker on ALL older posts!
Let's check `TimelineView.qml` or `PostDelegate.qml` to see how `ShowReadMarkerRole` is used.
