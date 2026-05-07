Ah! QML `ListView.section` groups items by the property.
`showReadMarker` is true for all posts older than the marker, and false for all posts newer!
So the section delegate is drawn BETWEEN the `false` section and the `true` section!
This means there is exactly ONE marker drawn in the timeline!
This is brilliant.

Wait! If we use numeric comparison, it will correctly set `showReadMarker` to true for all older posts and false for all newer posts.
But `findLatestPostId` currently ONLY checks the timeline.
If `m_initialLastReadId` is NOT in the timeline, `findLatestPostId({postId, m_initialLastReadId})` returns `postId`.
So `latestPostId` is `postId`.
And `latestPostId == m_initialLastReadId` is FALSE.
So `ShowReadMarkerRole` returns `false` for ALL posts!
This means the read marker is NEVER shown if the actual `m_lastReadId` post is missing from the timeline!
Even if the posts we fetched are older than `m_lastReadId`, no marker is shown.

So we should definitely replace `findLatestPostId` with a proper comparison!

Let's write a helper function `isNewerId` in `AbstractTimelineModel` or `TimelineModel`.
Or just use a free function in the cpp file.

```cpp
static bool isIdNewer(const QString &id1, const QString &id2) {
    if (id1.length() != id2.length()) {
        return id1.length() > id2.length();
    }
    return id1 > id2;
}
```

Then in `MainTimelineModel::updateReadMarker`:
```cpp
void MainTimelineModel::updateReadMarker(const QString &postId)
{
    const bool isHome = m_timelineName == QStringLiteral("home");

    if (isHome) {
        if (m_lastReadId.isEmpty() || isIdNewer(postId, m_lastReadId)) {
            m_account->saveTimelinePosition(QStringLiteral("home"), postId);
            m_lastReadId = postId;
        }
    }
}
```

Wait, `TimelineModel::findLatestPostId` is a public method (or protected).
```cpp
    [[nodiscard]] QString findLatestPostId(const QStringList &postIds) const;
```
It is currently defined in `src/timeline/timelinemodel.cpp`:
```cpp
QString TimelineModel::findLatestPostId(const QStringList &postIds) const
{
    if (postIds.isEmpty()) {
        return QString();
    }

    // Mastodon API documentation claims that a server is responsible for sorting IDs before generating the API
    // response: https://docs.joinmastodon.org/api/guidelines/#id
    // So, we just need to find a first post ID on m_timeline.
    for (const auto post : std::as_const(m_timeline)) {
        if (postIds.contains(post->originalPostId())) {
            return post->originalPostId();
        }
    }

    qWarning() << "None of postIds found. It shouldn't happen.";
    return QString();
}
```
Wait, if we can just implement this correctly:
```cpp
QString TimelineModel::findLatestPostId(const QStringList &postIds) const
{
    if (postIds.isEmpty()) {
        return QString();
    }

    QString latest = postIds.first();
    for (const auto &id : postIds) {
        if (id.length() > latest.length() || (id.length() == latest.length() && id > latest)) {
            latest = id;
        }
    }
    return latest;
}
```
If we do this, it doesn't even need to search the timeline!
It just returns the lexicographically largest ID, considering lengths.
This perfectly fixes both `updateReadMarker` and `ShowReadMarkerRole`!

Wait, is there any reason to search the timeline?
The comment says: "Mastodon API documentation claims that a server is responsible for sorting IDs before generating the API response... So, we just need to find a first post ID on m_timeline."
This means the author THOUGHT that since they are sorted, searching the timeline is the way to compare two IDs!
They didn't realize they could just compare them directly as strings (with length consideration).
By searching the timeline, they introduced a bug where if an ID is not in the timeline, it fails to find the correct maximum.

Let's check if the user's issue ("Why doesn't tokodon remember my last scroll position when I have the option enabled?") is solved by this.
If Tokodon saves the timeline position correctly, then `m_lastReadId` will be the actual newest post they read.
When they open the app, it fetches `m_lastReadId`.
It fetches OLDER posts, then NEWER posts.
Then it repositions to `pos` (which is the index of the first OLDER post).
Wait! If it repositions to the first OLDER post, then the view starts at the first OLDER post!
If the first OLDER post is post 99, and `m_lastReadId` is 100, then the view starts at 99.
Is that correct?
If you read up to 100, the next post you want to read is 101!
But 101 is above 100 in the timeline.
If the view starts at 99, you are seeing a post you ALREADY read! (Because 99 is older than 100).
So you have to scroll UP to see 100, 101, 102.
If the view starts at 99, and you scroll down, you see 98, 97, which you already read!
Wait. Why does Tokodon position at `pos`?
Let's see: `pos` is returned by `fetchedTimeline`.
`pos` is the number of NEW posts inserted!
Wait, if `pos` is the number of NEW posts inserted, and they are prepended, then index `pos` is exactly the item that was at index 0 BEFORE the insertion!
Before the insertion of NEWER posts, the timeline contained OLDER posts (e.g. 99, 98, 97).
So index 0 was 99!
When we prepend the NEWER posts (102, 101, 100), they become indices 0, 1, 2.
Index 3 is now 99.
`pos` is 3.
So `repositionAt(3)` positions at 99.
So we position at 99!
But the user wants to continue reading from where they left off! They want to start at 100!
Wait, if 100 is in the newer posts, shouldn't we position at 100?
But where is 100? It is the LAST item in the newer posts!
So its index is `pos - 1`!
If we do `repositionAt(pos - 1)`, we position at 100!
Wait! Is 100 ALWAYS the last item in the newer posts?
Yes, because `m_prev` (which is `min_id=100`) returns posts *newer* than 100.
Wait! If `min_id=100` returns posts NEWER than 100, then 100 is NOT in the newer posts!
Ah!
Let's check the Mastodon API documentation!
`min_id`: Return results immediately newer than this ID.
This means 100 is NOT included in the results!
So the newer posts are 102, 101.
And the older posts are 99, 98.
Where is 100?!
IT IS MISSING!
