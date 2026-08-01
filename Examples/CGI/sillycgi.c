#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
    This is from this simple tutorial https://www.eskimo.com/~scs/cclass/handouts/cgi.html about CGI with C language.
*/

int xctod(int c)
{
    if (isdigit(c))
        return c - '0';
    else if (isupper(c))
        return c - 'A' + 10;
    else if (islower(c))
        return c - 'a' + 10;
    else
        return 0;
}

char *unescstring(char *src, int srclen, char *dest, int destsize)
{
    char *endp = src + srclen;
    char *srcp;
    char *destp = dest;
    int nwrote = 0;

    for (srcp = src; srcp < endp; srcp++)
    {
        if (nwrote > destsize)
            return NULL;
        if (*srcp == '+')
            *destp++ = ' ';
        else if (*srcp == '%')
        {
            *destp++ = 16 * xctod(*(srcp + 1)) + xctod(*(srcp + 2));
            srcp += 2;
        }
        else
            *destp++ = *srcp;
        nwrote++;
    }

    *destp = '\0';

    return dest;
}

char *cgigetval(char *fieldname)
{
    int fnamelen;
    char *p, *p2, *p3;
    int len1, len2;
    static char *querystring = NULL;
    if (querystring == NULL)
    {
        querystring = getenv("QUERY_STRING");
        if (querystring == NULL)
            return NULL;
    }

    if (fieldname == NULL)
        return NULL;

    fnamelen = strlen(fieldname);

    for (p = querystring; *p != '\0';)
    {
        p2 = strchr(p, '=');
        p3 = strchr(p, '&');
        if (p3 != NULL)
            len2 = p3 - p;
        else
            len2 = strlen(p);

        if (p2 == NULL || p3 != NULL && p2 > p3)
        {
            /* no = present in this field */
            p += len2;
            continue;
        }
        len1 = p2 - p;

        if (len1 == fnamelen && strncmp(fieldname, p, len1) == 0)
        {
            /* found it */
            int retlen = len2 - len1 - 1;
            char *retbuf = malloc(retlen + 1);
            if (retbuf == NULL)
                return NULL;
            unescstring(p2 + 1, retlen, retbuf, retlen + 1);
            return retbuf;
        }

        p += len2;
        if (*p == '&')
            p++;
    }

    /* never found it */

    return NULL;
}
void upperstring(char *str)
{
    char *p;

    for (p = str; *p != '\0'; p++)
    {
        if (islower(*p))
            *p = toupper(*p);
    }
}

void lowerstring(char *str)
{
    char *p;

    for (p = str; *p != '\0'; p++)
    {
        if (isupper(*p))
            *p = tolower(*p);
    }
}

void reverse(char *str)
{
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++)
    {
        str[i] = str[len - i - 1];
    }
}

main()
{
    char *browser;
    char *edittype;
    char *text;

    printf("Content-Type: text/html\n\n");

    printf("<html>\n");
    printf("<head>\n");
    printf("<title>CGI test result</title>\n");
    printf("</head>\n");
    printf("<body>\n");

    browser = getenv("HTTP_USER_AGENT");

    printf("<p>\n");
    printf("Hello, ");
    if (browser != NULL && strstr(browser, "Lynx") != NULL)
        printf("Lynx");
    else if (browser != NULL && strstr(browser, "Mosaic") != NULL)
        printf("Mosaic");
    else if (browser != NULL && strstr(browser, "Mozilla") != NULL)
        printf("Netscape");
    else
        printf("unknown browser");
    printf(" user!\n");

    printf("<p>\n");

    printf("Here is your modified text:\n");
    printf("<br>\n");

    text = cgigetval("textfield");
    edittype = cgigetval("edittype");

    if (text == NULL)
    {
        printf("You didn't enter any text!\n");
    }
    else if (edittype != NULL && strcmp(edittype, "reverse") == 0)
    {
        reverse(text);
        printf("%s\n", text);
    }
    else if (edittype != NULL && strcmp(edittype, "upper") == 0)
    {
        upperstring(text);
        printf("%s\n", text);
    }
    else if (edittype != NULL && strcmp(edittype, "lower") == 0)
    {
        lowerstring(text);
        printf("%s\n", text);
    }
    else
    {
        printf("You didn't select a transformation!\n");
    }

    printf("</body>\n");
    printf("</html>\n");
}