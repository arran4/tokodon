Let's check if the `repositionAt(pos)` index needs adjusting.
In `src/timeline/maintimelinemodel.cpp`:
```cpp
            if (publicTimelines.contains(m_timelineName) && backwards) {
                int const pos = fetchedTimeline(data);
                Q_EMIT repositionAt(pos);
            } else {
                fetchedTimeline(data, true);
            }
```
If `fetchedTimeline(data)` prepends posts, `pos` is the number of inserted posts.
And `Q_EMIT repositionAt(pos)` is called.
Wait! If `pos` posts were inserted, the post that was at index 0 (which was the first *older* post) is now at index `pos`.
If the user wants to continue reading from `m_lastReadId`, and `m_lastReadId` was in the *newer* posts we just inserted.
Since we inserted them, the newer posts are at indices 0, 1, ..., `pos - 1`.
Because `min_id` returns the oldest posts first (since it inverts sort order?), NO, wait!
Let's check `fetchedTimeline(data)`:
```cpp
    for (auto &post : posts) {
```
The Mastodon API `min_id` returns results in chronological order (oldest first)!
Wait! Let's check `fetchedTimeline(data)`.
```cpp
        } else {
            const auto postOld = m_timeline.first();
            const auto postNew = posts.first();
            if (postOld->originalPostId() > postNew->originalPostId()) {
                const int row = m_timeline.size();
                const int last = row + posts.size() - 1;
                beginInsertRows({}, row, last);
                m_timeline += posts;
                endInsertRows();
            } else {
                beginInsertRows({}, 0, posts.size() - 1);
                m_timeline = posts + m_timeline;
                endInsertRows();
            }
        }
```
Wait! `m_timeline = posts + m_timeline;`
This assumes `posts` is sorted newest to oldest!
If the API returns oldest first when using `min_id`, then `posts` is sorted OLDEST to NEWEST!
If it is sorted oldest to newest, then `posts.first()` is the OLDEST post in the new batch!
If `posts.first()` is the oldest post, then `postOld->originalPostId() > postNew->originalPostId()` could be TRUE!
Let's trace this carefully:
`postOld` is the first of the older posts (e.g. 99).
`posts` contains the newer posts (e.g. 100, 101, 102).
If the API returns them oldest first: `posts = [100, 101, 102]`.
`postNew = 100`.
`postOld->originalPostId() > postNew->originalPostId()`: `99 > 100` -> FALSE!
So it goes to `else`:
`m_timeline = posts + m_timeline;`
So the timeline becomes: `[100, 101, 102, 99, 98]`.
Wait! This is WRONG!
The timeline should be NEWEST to OLDEST! `[102, 101, 100, 99, 98]`.
If `m_timeline` is `[100, 101, 102, 99, 98]`, then it's NOT sorted correctly!

Does Mastodon API return oldest first when using `min_id`?
"Return results immediately newer than this ID. Inverts the sort order."
Yes, Mastodon `min_id` returns results in chronological order (oldest first).
Wait, if it does, why doesn't `fetchedTimeline` reverse it?
Let's check Tokodon's implementation. Does it reverse `min_id` responses?
Wait! In Tokodon, `min_id` is NOT used by `m_prev`!
Let's check `getPrevLink`:
```cpp
std::optional<QUrl> TextHandler::getPrevLink(const QString &linkText)
{
    const auto match = TextRegex::prevLink.match(linkText);
...
```
In `TextHandlerTest.cpp`:
`<https://mastodon.social/api/v1/timelines/home?min_id=106041072990666170>; rel="prev", <https://mastodon.social/api/v1/timelines/home?max_id=106041042790757962>; rel="next"`
So `prev` uses `min_id`!
If `min_id` is used, the Mastodon API returns them OLDEST first!
But Tokodon `fetchedTimeline` does NOT reverse them!
Wait! Is it true that `min_id` inverts sort order?
Let's check the API documentation:
https://docs.joinmastodon.org/api/guidelines/#pagination
"min_id: Return results immediately newer than this ID. Inverts the sort order (sorts by oldest first)."
Yes, it does!
Wait! But wait, `fetchedTimeline` is:
```cpp
    std::ranges::transform(std::as_const(array), std::back_inserter(posts), [this](const QJsonValue &value) -> Post * {
...
```
Does Tokodon just shove them into `posts`? Yes!
If Tokodon doesn't reverse them, they are displayed OUT OF ORDER!
Wait! Does Tokodon actually have this bug?
Let's look at Tokodon's UI when you scroll up. When you scroll up, it fetches `m_prev`.
If it doesn't reverse them, the new posts are out of order!
Let's check if there is any reverse logic.
Maybe `TimelineModel` or `MainTimelineModel` handles reversing?
No, it just does `m_timeline = posts + m_timeline;`.
BUT wait, look at `fetchedTimeline` again:
```cpp
            if (postOld->originalPostId() > postNew->originalPostId()) {
```
Wait! If `postOld = 99`, `postNew = 100`, `postOld > postNew` is FALSE!
Wait, if they are out of order, how does it work?
If they are out of order, wouldn't people complain?
Actually, `min_id` was historically NOT reversing sort order in older Mastodon versions. It started reversing in Mastodon 2.6.
Wait, no. The Mastodon API has a `since_id` and a `min_id`.
`min_id` was added specifically to return oldest first, so clients can just reverse and append to the top.
Wait, if Tokodon has a bug where it doesn't reverse `min_id` results, that's a big issue!
Wait, but wait! In `fillTimeline` there's a comment:
```cpp
    // TODO: this is an *upper bound* so it always is one less than the last post we read
    // is this really how it's supposed to work wrt read markers?
```
Wait, we shouldn't fix something that we aren't 100% sure is broken unless it's strictly necessary for the read marker.
If the read marker is our main goal, then does `repositionAt` index need fixing?
If we `repositionAt(pos)`, we jump to index `pos`.
If `pos = posts.size()`, we jump to index `posts.size()`.
The index `posts.size()` is the FIRST post from the older posts.
If we want to continue reading, we should jump to the NEWEST post that we have ALREADY read, or the OLDEST unread post!
If we jump to the OLDEST unread post, it should be the post immediately above the one we already read!
Since `pos` is the index of the post we already read (wait, is index `pos` the one we read? No, index `pos` is the post older than `m_lastReadId`).
If we jump to index `pos - 1`, we jump to the OLDEST unread post! (Or the actual `m_lastReadId` if it is included).
Wait! If `min_id` returns oldest first: `[100, 101, 102]`.
Then index `pos - 1` is 102!
Wait, if `m_timeline` is `[100, 101, 102, 99, 98]`, then index `pos - 1` (index 2) is 102!
Is 102 the oldest unread post? No, 100 is the oldest unread post! (Actually 100 is the read post).
So if the array is `[100, 101, 102]`, index 0 is 100!
If we want to see 100, we should jump to index 0!
But Tokodon jumps to index 3 (which is 99)!
If we jump to index 3, we see 99.

Wait! If Tokodon doesn't reverse the array, then the timeline is `[100, 101, 102, 99, 98]`.
If we jump to index `pos - 1`, we jump to 102.
Let's see if Tokodon actually reverses it.
