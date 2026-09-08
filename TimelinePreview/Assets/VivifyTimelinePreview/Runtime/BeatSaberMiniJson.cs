using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace VivifyTimelinePreview
{

// Tiny dependency-free JSON parser for Unity 2019-compatible projects.
public static class BeatSaberMiniJson
{
    public static object Deserialize(string json)
    {
        if (string.IsNullOrEmpty(json)) return null;
        using (var parser = new Parser(json)) return parser.ParseValue();
    }

    private sealed class Parser : IDisposable
    {
        private readonly StringReader reader;
        public Parser(string json) { reader = new StringReader(json); }
        public void Dispose() { reader.Dispose(); }

        public object ParseValue()
        {
            EatWhitespace();
            int p = reader.Peek();
            if (p == -1) return null;
            char c = (char)p;
            if (c == '{') return ParseObject();
            if (c == '[') return ParseArray();
            if (c == '"') return ParseString();
            if (c == '-' || char.IsDigit(c)) return ParseNumber();
            string word = ParseWord();
            if (word == "true") return true;
            if (word == "false") return false;
            if (word == "null") return null;
            return null;
        }

        private Dictionary<string, object> ParseObject()
        {
            var dict = new Dictionary<string, object>();
            reader.Read();
            while (true)
            {
                EatWhitespace();
                if (reader.Peek() == '}') { reader.Read(); break; }
                string key = ParseString();
                EatWhitespace();
                if (reader.Read() != ':') throw new Exception("Invalid JSON object");
                object value = ParseValue();
                dict[key] = value;
                EatWhitespace();
                int next = reader.Read();
                if (next == '}') break;
                if (next != ',') throw new Exception("Invalid JSON object separator");
            }
            return dict;
        }

        private List<object> ParseArray()
        {
            var list = new List<object>();
            reader.Read();
            while (true)
            {
                EatWhitespace();
                if (reader.Peek() == ']') { reader.Read(); break; }
                list.Add(ParseValue());
                EatWhitespace();
                int next = reader.Read();
                if (next == ']') break;
                if (next != ',') throw new Exception("Invalid JSON array separator");
            }
            return list;
        }

        private string ParseString()
        {
            var sb = new StringBuilder();
            if (reader.Read() != '"') throw new Exception("Invalid JSON string");
            while (true)
            {
                int n = reader.Read();
                if (n == -1) break;
                char c = (char)n;
                if (c == '"') break;
                if (c != '\\') { sb.Append(c); continue; }
                int esc = reader.Read();
                if (esc == -1) break;
                switch ((char)esc)
                {
                    case '"': sb.Append('"'); break;
                    case '\\': sb.Append('\\'); break;
                    case '/': sb.Append('/'); break;
                    case 'b': sb.Append('\b'); break;
                    case 'f': sb.Append('\f'); break;
                    case 'n': sb.Append('\n'); break;
                    case 'r': sb.Append('\r'); break;
                    case 't': sb.Append('\t'); break;
                    case 'u':
                        var hex = new char[4];
                        for (int i = 0; i < 4; i++) hex[i] = (char)reader.Read();
                        sb.Append((char)Convert.ToInt32(new string(hex), 16));
                        break;
                }
            }
            return sb.ToString();
        }

        private object ParseNumber()
        {
            string token = ParseWhile(ch => ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E' || char.IsDigit(ch));
            if (token.IndexOf('.') < 0 && token.IndexOf('e') < 0 && token.IndexOf('E') < 0)
            {
                long l;
                if (long.TryParse(token, NumberStyles.Integer, CultureInfo.InvariantCulture, out l)) return l;
            }
            double d;
            if (double.TryParse(token, NumberStyles.Float, CultureInfo.InvariantCulture, out d)) return d;
            return 0d;
        }

        private string ParseWord() { return ParseWhile(ch => char.IsLetter(ch)); }
        private string ParseWhile(Func<char, bool> predicate)
        {
            var sb = new StringBuilder();
            while (reader.Peek() != -1 && predicate((char)reader.Peek())) sb.Append((char)reader.Read());
            return sb.ToString();
        }
        private void EatWhitespace() { while (reader.Peek() != -1 && char.IsWhiteSpace((char)reader.Peek())) reader.Read(); }
    }
}
}
