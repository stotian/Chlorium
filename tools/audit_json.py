"""Read consecutive audit JSON objects, including historical pretty inventories."""
import json


def read_events(path, allow_partial_tail=False):
    if not path.exists():
        return []
    text = path.read_text(encoding='utf-8-sig')
    decoder = json.JSONDecoder()
    result = []
    position = 0
    while position < len(text):
        while position < len(text) and text[position].isspace():
            position += 1
        if position == len(text):
            break
        try:
            event, end = decoder.raw_decode(text, position)
        except json.JSONDecodeError:
            if allow_partial_tail:
                break
            raise
        if not isinstance(event, dict) or not isinstance(event.get('event'), str):
            raise ValueError(f'{path}: invalid audit event at {position}')
        result.append(event)
        position = end
    return result
