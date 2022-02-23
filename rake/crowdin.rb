require "uri"
require "net/http"
require "json"
require "time"

# Set the crowdin parameters in .buildenv
#CROWDIN_API=
#CROWDIN_PROJECTID=
#CROWDIN_FILEID=

projectId = ENV["CROWDIN_PROJECTID"] || '485053'
apiKey = ENV["CROWDIN_API"]

# The File ID for the pia_desktop.ts file
# Note that the only way to get is by calling the API
# $ curl 'https://api.crowdin.com/api/v2/projects/$CROWDIN_PROJECTID/files' --header "Authorization: Bearer $CROWDIN_API_TOKEN"
# The resulting JSON has an `id` field
# You can use pipe the curl output to `jq '.data[0].data.id'` to get the id

fileId = ENV["CROWDIN_FILEID"] || '741'

@gitlab_token = ENV["CI_JOB_TOKEN"]
@gitlab_mr_id = ENV["CI_MERGE_REQUEST_ID"]
@gitlab_project_id = ENV["CI_MERGE_REQUEST_PROJECT_ID"]

task :tspull => [ :export ] do |t|
  if(apiKey && projectId)
    puts "Using crowdin project ID #{projectId}"
  end
end

task :tspush => [ :export ] do |t| 
  if(apiKey && projectId)
    puts "Using crowdin project ID #{projectId}"

    exportBuild = Build.new('translations_export')
    sourceFile = exportBuild.artifact('pia_desktop.ts')
    sourceContent = File.read(sourceFile, encoding: "UTF-8")

    puts "Creating storage on crowdin"
    url = URI("https://api.crowdin.com/api/v2/storages")
    https = Net::HTTP.new(url.host, url.port)
    https.use_ssl = true
    request = Net::HTTP::Post.new(url)
    request["Authorization"] = "Bearer #{apiKey}"
    request["Crowdin-API-FileName"] = "pia_desktop.ts"
    request.body = sourceContent

    response = https.request(request)
    result = JSON.parse(response.read_body)
    storageId = result["data"]["id"]
    puts "Storage ID is #{storageId}"

    puts "Updating file on crowdin"
    url = URI("https://api.crowdin.com/api/v2/projects/#{projectId}/files/#{fileId}")
    https = Net::HTTP.new(url.host, url.port)
    https.use_ssl = true

    request = Net::HTTP::Put.new(url)
    request["Authorization"] = "Bearer #{apiKey}"
    request["Content-Type"] = "application/json"
    request.body = JSON.dump({
      "storageId": storageId
    })

    response = https.request(request)
    puts response.read_body
  else
    puts "ERROR: Missing crowdin API and ID."
  end
end

def build_index(doc)
  contextItems = doc.xpath("//context")
  index = {}
  
  contextItems.each do |item|
    contextName = item.xpath(".//name").text
    sourceItems = item.xpath(".//source")

    sourceItems.each do |src|
      sourceText = src.text
      keyString = "#{contextName} / #{sourceText}"
      index[keyString] = {
        text: sourceText,
        context: contextName
      }
    end
  end

  return index
end

def diff_ts(newDoc, oldDoc)

  newIndex = build_index(newDoc)
  oldIndex = build_index(oldDoc)

  newKeys = newIndex.keys
  oldKeys = oldIndex.keys

  timestamp = Time.now.utc.iso8601

  out = "```\ntsdiff:\n=========\n\n"

  out += "New in LOCAL: \n"
  out += newKeys.difference(oldKeys).pretty_inspect
  out += "Present in REMOTE but missing in LOCAL: \n"
  out += oldKeys.difference(newKeys).pretty_inspect 
  out += "\n\nTSDIFFOUT Generated #{timestamp} \n"
  out += "```"

  [out, newKeys.difference(oldKeys).size + oldKeys.difference(newKeys).size]
end

def add_comment(content)
  puts "Creating a new comment"
  url = URI("https://gitlab.kape.com/api/v4/projects/#@gitlab_project_id/merge_requests/#@gitlab_mr_id/notes")

  https = Net::HTTP.new(url.host, url.port)
  https.use_ssl = true

  request = Net::HTTP::Post.new(url)
  request["Private-Token"] = @gitlab_token
  request["Content-Type"] = "application/json"
  request.body = JSON.dump({
    "body": content
  })

  response = https.request(request)
  puts "Gitlab response:"
  puts response
end

def find_comment(marker)
  url = URI("https://gitlab.kape.com/api/v4/projects/#@gitlab_project_id/merge_requests/#@gitlab_mr_id/notes")

  https = Net::HTTP.new(url.host, url.port)
  https.use_ssl = true

  request = Net::HTTP::Get.new(url)
  request["Private-Token"] = @gitlab_token

  response = https.request(request)
  result = JSON.parse(response.read_body)
  result.each do |comment| 
    if comment["body"].include?(marker)
      return comment["id"]
    end
  end

  return nil
end

def update_comment(id, content)
  puts "Updating comment"
  url = URI("https://gitlab.kape.com/api/v4/projects/#@gitlab_project_id/merge_requests/#@gitlab_mr_id/notes/#{id}")

  https = Net::HTTP.new(url.host, url.port)
  https.use_ssl = true

  request = Net::HTTP::Put.new(url)
  request["Private-Token"] = @gitlab_token
  request["Content-Type"] = "application/json"
  request.body = JSON.dump({
    "body": content
  })

  response = https.request(request)
  puts "Gitlab response:"
  puts response
end

def gitlab_create_or_replace(content, marker)
  existing_comment = find_comment(marker)
  puts "Found comment: ",existing_comment
  if existing_comment.nil?
    add_comment(content)
  else
    update_comment(existing_comment, content)
  end

end

task :tsdiff => [ :export ] do |t|
  if apiKey.nil? 
    puts "Missing CROWDIN_API. Skipping tsdiff"
  else
    require "nokogiri"
    require "pp"
    require "open-uri"

    puts "Parsing new pia_desktop.ts"
    exportBuild = Build.new('translations_export')
    sourceFile = exportBuild.artifact('pia_desktop.ts')
    newContent = File.read(sourceFile, encoding: "UTF-8")

    puts "Fetching source info from crowdin"
    url = URI("https://api.crowdin.com/api/v2/projects/#{projectId}/files/#{fileId}/download")
    https = Net::HTTP.new(url.host, url.port)
    https.use_ssl = true
    request = Net::HTTP::Get.new(url)
    request["Authorization"] = "Bearer #{apiKey}"

    response = https.request(request)
    result = JSON.parse(response.read_body)

    oldUrl = result["data"]["url"]

    out = diff_ts(Nokogiri::XML(newContent), Nokogiri::XML(URI.open(oldUrl)))

    out_string = out[0]
    diff_count = out[1]
    puts out_string


    if ENV["CI_MERGE_REQUEST_ID"]
      puts "Skipping gitlab comment..."
      # gitlab_create_or_replace(out_string, "TSDIFFOUT")
    end
  end
end
