# OpenRISC Linux CI build branch

This branch just has the github ci commits on top of the main
branch.

Its a bit of a hack but we dont want travis files to get into
upstream so I thought this way is good. If there is a better
way let me know.

To update latest code:

```
git checkout travis
git rebase -i for-next
git push origin travis
```
