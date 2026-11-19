# VERSION: 1.00
# AUTHORS: qBittorrentAccess (https://reaperaccessible.fr)

# Search plugin for plugintorrent.com (audio plugins, sample packs, libraries, DAWs).
# The site is a WordPress blog: its public REST API returns each post with its content,
# which holds the magnet link and the "Size:" line, so one request gives a whole page of results.

import html
import json
import re
from datetime import datetime, timezone
from urllib.parse import quote, unquote

from helpers import retrieve_url
from novaprinter import prettyPrinter


class plugintorrent(object):
    url = 'https://plugintorrent.com'
    name = 'PluginTorrent'
    # WordPress category ids of the site
    supported_categories = {
        'all': '',
        'music': '121,21417,103,766',               # Audio Sample, Music Sample, Library, Templates & Presets
        'software': '2,15,9,8,15455,47,71,4559',    # VST, VST3, AU, AAX, CLAP, DAW, Standalone, Portable
    }

    PER_PAGE = 100
    MAX_PAGES = 3

    magnet_re = re.compile(r'href="(magnet:\?[^"]+)"', re.IGNORECASE)
    torrent_re = re.compile(r'href="(https?://[^"]+\.torrent)"', re.IGNORECASE)
    # The size is written three ways: exact bytes in a span title, "Size: 802.2 MB",
    # or a bare "15.82 GB" alone in a list item
    bytes_re = re.compile(r'id="tor-size-humn"\s+title="(\d+)"')
    size_re = re.compile(r'Size\s*:\s*([\d.,]+\s*[KMGT]i?B)', re.IGNORECASE)
    li_size_re = re.compile(r'<li>\s*([\d.,]+\s*[KMGT]i?B)\s*</li>', re.IGNORECASE)
    tag_re = re.compile(r'<[^>]+>')

    def search(self, what, cat='all'):
        # nova2 hands the query already percent-encoded
        query = quote(unquote(what))
        categories = self.supported_categories.get(cat, '')
        for page in range(1, self.MAX_PAGES + 1):
            api = (f"{self.url}/wp-json/wp/v2/posts?search={query}&per_page={self.PER_PAGE}"
                   f"&page={page}&orderby=relevance&_fields=link,title,date_gmt,content")
            if categories:
                api += f"&categories={categories}"
            # Keep the JSON intact: entities are decoded field by field below
            data = retrieve_url(api, unescape_html_entities=False)
            try:
                posts = json.loads(data)
            except ValueError:
                return
            if not isinstance(posts, list):
                return
            for post in posts:
                self.print_post(post)
            if len(posts) < self.PER_PAGE:
                return

    def print_post(self, post):
        content = post.get('content', {}).get('rendered', '')
        found = self.magnet_re.search(content) or self.torrent_re.search(content)
        if not found:
            return
        size = self.bytes_re.search(content)
        if size:
            size = size.group(1) + ' B'
        else:
            text = html.unescape(content).replace('\xa0', ' ')
            size = (self.size_re.search(self.tag_re.sub(' ', text))
                    or self.li_size_re.search(text))
            size = size.group(1).replace(',', '.') if size else '-1'
        pub_date = -1
        try:
            pub_date = int(datetime.fromisoformat(post['date_gmt']).replace(tzinfo=timezone.utc).timestamp())
        except (KeyError, ValueError, TypeError):
            pass
        prettyPrinter({
            'link': html.unescape(found.group(1)),
            'name': html.unescape(post.get('title', {}).get('rendered', '')).strip(),
            'size': size,
            'seeds': -1,
            'leech': -1,
            'engine_url': self.url,
            'desc_link': post.get('link', self.url),
            'pub_date': pub_date,
        })
