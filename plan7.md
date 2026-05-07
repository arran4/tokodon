Since I don't see `std::reverse`, I will assume Tokodon just prepends them as-is.
If `posts` contains `[100, 101, 102]`, and it does `m_timeline = posts + m_timeline`, the timeline is `100, 101, 102, 99, 98`.
If it jumps to `pos`, it jumps to 99.
Wait, if it jumps to `pos`, the user sees 99.
If the user scrolls up, they see 102! Then 101! Then 100!
Wait, that means if they scroll up, they see the NEWEST post first! (102). Then older (101). Then 100!
This is completely backwards!
But wait... if `m_prev` uses `min_id`, DOES the Mastodon API return oldest first?
Ah, the Mastodon API docs say:
"min_id: ... Inverts the sort order (sorts by oldest first)."
BUT the Mastodon API might have changed, or maybe Tokodon actually uses `since_id`?
Let's check `getPrevLink` again. The regex for `prevLink`:
```cpp
const QRegularExpression prevLink(QStringLiteral("<(http[s]?://[^>]+)>;\\s*rel=\"prev\""));
```
It just extracts the URL! Whatever URL the server provides for `rel="prev"`, Tokodon uses.
The server usually provides `min_id` for `rel="prev"`.
If the server returns oldest first, and Tokodon prepends it, then the newest posts are at the bottom of the prepended block!
So the timeline is: `100 (oldest new), 101, 102 (newest new), 99 (old), 98 (older)`.
If this happens, it's a known bug in Tokodon that is out of scope for the current issue.

Let's focus on the issue: "Why doesn't tokodon remember my last scroll position when I have the option enabled?".
By fixing `findLatestPostId`, Tokodon NO LONGER overwrites `m_lastReadId` with 99 on startup!
So `m_lastReadId` stays 100!
And Tokodon positions the view at `pos` (which is 99).
Wait, if Tokodon positions the view at 99, the user is looking at 99!
If the user wants to see 100, they have to scroll up.
Wait, if they scroll up, they see 102, 101, 100.
If they see 102, then `updateReadMarker(102)` is called!
Then `m_lastReadId` becomes 102!
So they immediately skip 100 and 101!
Is this expected? Yes, if they jump over posts by scrolling fast.
But the user says Tokodon DOES NOT REMEMBER their scroll position!
By fixing `findLatestPostId`, we fixed the bug where it FORGETS the scroll position!
Because before, it would overwrite it to 99, so the NEXT time you open it, it loads 99, and overwrites it to 98!
So it was constantly sliding down the timeline!

Is the index `pos` correct for `repositionAt`?
If `pos` points to 99, then 99 is at the top of the view.
If you were at 100, then 99 is exactly the post BELOW 100.
In a reverse-chronological timeline, 100 is above 99.
So if 99 is at the top of the view, 100 is just out of view at the top!
So if you scroll up slightly, you see 100!
This is PERFECT! It means you are exactly where you left off (you read up to 100, so you are looking at the post just below 100).
When you scroll up, you reveal 100, then 101, etc.
So `repositionAt(pos)` is EXACTLY CORRECT.

Wait, is there any case where `pos` is wrong?
What if `alwaysAppendToEnd` is true? No, `alwaysAppendToEnd` is false.
So `repositionAt(pos)` works as intended.
The whole reason the scroll position was not remembered was because `m_lastReadId` was aggressively and incorrectly overwritten by `findLatestPostId` due to the older posts loading first and the newer posts not being in the timeline yet.
